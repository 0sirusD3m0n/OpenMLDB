package com._4paradigm.fesql.common;
import com._4paradigm.fesql.FeSqlLibrary;

import com._4paradigm.fesql.node.ColumnRefNode;
import com._4paradigm.fesql.node.ExprListNode;
import com._4paradigm.fesql.node.ExprNode;
import com._4paradigm.fesql.node.ExprType;
import com._4paradigm.fesql.tablet.Tablet;
import com._4paradigm.fesql.type.TypeOuterClass;
import com._4paradigm.fesql.vm.*;
import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonParser;
import lombok.Data;
import org.apache.commons.io.FileUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.apache.commons.lang3.StringUtils;

import java.io.File;
import java.io.IOException;
import java.util.*;

public class DDLEngine {
    static {
        // Ensure native initialized
        FeSqlLibrary.initCore();
        Engine.InitializeGlobalLLVM();
    }

    private static final Logger logger = LoggerFactory.getLogger(DDLEngine.class);

    public static String SQLTableName = "sql_table";

    public static int resolveColumnIndex(ExprNode expr, PhysicalOpNode planNode) throws FesqlException {
        if (expr.getExpr_type_() == ExprType.kExprColumnRef) {
            int index = CoreAPI.ResolveColumnIndex(planNode, ColumnRefNode.CastFrom(expr));
            if (index >= 0 && index <= planNode.GetOutputSchema().size()) {
                return index;
            } else {
                throw new FesqlException("Fail to resolve {} with index = {}".format(expr.GetExprString(), index));
            }
        }
        throw new FesqlException("Expr {} not supported".format(expr.GetExprString()));
    }

    /**
     *
     * @param sql
     * @param schema json format
     * @return
     */
    public static String genDDL(String sql, String schema) {
        String tempDB = "temp_" + System.currentTimeMillis();
        TypeOuterClass.Database.Builder db = TypeOuterClass.Database.newBuilder();
        db.setName(tempDB);
        List<TypeOuterClass.TableDef> tables = getTableDefs(schema);
        Map<String, TypeOuterClass.TableDef> tableDefMap = new HashMap<>();
//        tableDefMap.forEach((k, v) -> System.out.printf("xx"));
        for (TypeOuterClass.TableDef e : tables) {
            db.addTables(e);
            tableDefMap.put(e.getName(), e);
        }
        try {
            RequestEngine engine = new RequestEngine(sql, db.build());
            // SQLEngine engine = new SQLEngine(sql, db.build());
            PhysicalOpNode plan = engine.getPlan();
            List<PhysicalOpNode> listNodes = new ArrayList<PhysicalOpNode>();
            dagToList(plan, listNodes);
//            printDagListInfo(listNodes);
            Map<String, RtidbTable> rtidbTables = parseRtidbIndex(listNodes, tableDefMap);
            StringBuilder sb = new StringBuilder();
            for (Map.Entry<String, RtidbTable> e : rtidbTables.entrySet()) {
                if (e.getKey().equals(e.getValue().getTableName())) {
                    sb.append(e.getValue().toDDL());
                    sb.append(";\n");
                }

            }
            String res = sb.toString();
            logger.info("gen ddl:" +  res);
            return res;
        } catch (UnsupportedFesqlException | FesqlException e) {
            e.printStackTrace();
        }
        return "";
    }
    /**
     * 只对window op做解析，因为fesql node类型太多了，暂时没办法做通用性解析
     */
    public static void parseWindowOp(PhysicalOpNode node, Map<String, RtidbTable> rtidbTables) {
        logger.info("begin to pares window op");
        PhysicalRequestUnionNode castNode = PhysicalRequestUnionNode.CastFrom(node);
        long start = -1;
        long end = -1;
        long cntStart = -1;
        long cntEnd = -1;
        if (castNode.window().range().frame().frame_range() != null) {
            start = Math.abs(Long.valueOf(castNode.window().range().frame().frame_range().start().GetExprString()));
            end = Math.abs(Long.valueOf(castNode.window().range().frame().frame_range().end().GetExprString()));
        } else {
            cntStart = Long.valueOf(castNode.window().range().frame().frame_rows().start().GetExprString());
            cntEnd = Long.valueOf(castNode.window().range().frame().frame_rows().end().GetExprString());
        }
        List<PhysicalOpNode> nodes = new ArrayList<>();
        for (int i = 0; i < castNode.GetProducerCnt(); i++) {
            nodes.add(castNode.GetProducer(i));
        }
        for (int i = 0; i < castNode.window_unions().GetSize(); i++) {
            nodes.add(castNode.window_unions().GetKey(i));
        }

//        int nodeIndex = -1;
        for (PhysicalOpNode e : nodes) {
//            nodeIndex++;
            PhysicalDataProviderNode unionTable = findDataProviderNode(e);
            logger.info("union table = {}", unionTable.GetName());

            String table = unionTable.GetName();
            RtidbTable rtidbTable = rtidbTables.get(table);

            RtidbIndex index = new RtidbIndex();
            List<String> keys = index.getKeys();
            for (int keyIndex = 0; keyIndex < castNode.window().partition().keys().GetChildNum(); keyIndex++) {
                String key = CoreAPI.ResolveSourceColumnName(e,
                        ColumnRefNode.CastFrom(castNode.window().partition().keys().GetChild(keyIndex)));
                keys.add(key);
            }
            String ts = CoreAPI.ResolveSourceColumnName(e, ColumnRefNode.CastFrom(castNode.window().sort().orders().order_by().GetChild(0)));

            index.setTs(ts);
            if (start != -1) {
                if (start < 60 * 1000) {
                    // 60秒
                    index.setExpire(60 * 1000);
                } else {
                    index.setExpire(start);
                }
            }
            if (cntStart != -1) {
                index.setAtmost(cntStart);
            }
            if (index.getAtmost() > 0 && index.getExpire() == 0) {
                index.setType(TTLType.kLatest);
            }
            if (index.getAtmost() == 0 && index.getExpire() > 0) {
                index.setType(TTLType.kAbsolute);
            }
            rtidbTable.addIndex(index);
        }
        logger.info("end to pares window op");
    }

    public static void parseLastJoinOp(PhysicalOpNode node, Map<String, RtidbTable> rtidbTables) {
        logger.info("begin to pares lastjoin op");
        PhysicalRequestJoinNode join = PhysicalRequestJoinNode.CastFrom(node);
        PhysicalDataProviderNode dataNode = findDataProviderNode(join.GetProducer(0));
        String leftName = "";
        String rightName = "";
        if (dataNode != null) {
            // PhysicalDataProviderNode dataNode = PhysicalDataProviderNode.CastFrom(join.GetProducer(0));
            logger.info(dataNode.GetName());
            leftName = dataNode.GetName();
        }
        dataNode = findDataProviderNode(join.GetProducer(1));
        if (dataNode != null) {
            // PhysicalDataProviderNode dataNode = PhysicalDataProviderNode.CastFrom(join.GetProducer(0));
            logger.info(dataNode.GetName());
            rightName = dataNode.GetName();
        } else {
            return;
        }
        // RtidbTable leftTable = rtidbTables.get(leftName);
        RtidbTable rightTable = rtidbTables.get(rightName);
        // RtidbIndex leftIndex = new RtidbIndex();
        RtidbIndex rightIndex = new RtidbIndex();

        Key conditionKey = join.join().right_key();
        Sort sort = join.join().right_sort();
        if (sort != null && sort.orders() != null) {
            String ts = CoreAPI.ResolveSourceColumnName(join, ColumnRefNode.CastFrom(sort.orders().order_by().GetChild(0)));
            rightIndex.setTs(ts);
        }
        rightIndex.setAtmost(1);
        List<String> keys = rightIndex.getKeys();
        for (int i = 0; i < conditionKey.keys().GetChildNum(); i++) {
            String keyName = CoreAPI.ResolveSourceColumnName(node,
                    ColumnRefNode.CastFrom(conditionKey.keys().GetChild(i)));
            keys.add(keyName);
        }
        rightIndex.setType(TTLType.kLatest);
        rightTable.addIndex(rightIndex);
        logger.info("begin to pares lastjoin op");
    }

    public static Map<String, RtidbTable> parseRtidbIndex(List<PhysicalOpNode> nodes, Map<String, TypeOuterClass.TableDef> tableDefMap) throws FesqlException {
        Map<String, RtidbTable> rtidbTables = new HashMap<>();
        Map<String, String> table2OrgTable = new HashMap<>();
        for (PhysicalOpNode node : nodes) {
            PhysicalOpType type = node.GetOpType();
            if (type.swigValue() == PhysicalOpType.kPhysicalOpDataProvider.swigValue()) {
                PhysicalDataProviderNode castNode = PhysicalDataProviderNode.CastFrom(node);
                RtidbTable rtidbTable = rtidbTables.get(castNode.GetName());
                if (rtidbTable == null) {
                    rtidbTable = new RtidbTable();
                    rtidbTable.setTableName(castNode.GetName());
                    rtidbTable.setSchema(tableDefMap.get(castNode.GetName()));
                    rtidbTables.put(castNode.GetName(), rtidbTable);
                }
                continue;
            }
            if (type.swigValue() == PhysicalOpType.kPhysicalOpRequestUnoin.swigValue()) {
                parseWindowOp(node, rtidbTables);
                continue;
            }
            if (type.swigValue() == PhysicalOpType.kPhysicalOpRequestJoin.swigValue()) {
                parseLastJoinOp(node, rtidbTables);
                continue;
            }
            if (type.swigValue() == PhysicalOpType.kPhysicalOpLimit.swigValue()) {
                continue;
            }
            if (type.swigValue() == PhysicalOpType.kPhysicalOpRename.swigValue()) {
                PhysicalRenameNode castNode = PhysicalRenameNode.CastFrom(node);
                logger.info("rename = " + castNode.getName_());
                PhysicalDataProviderNode dataNode = findDataProviderNode(node.GetProducer(0));
                if (dataNode != null) {
                    table2OrgTable.put(castNode.getName_(), dataNode.GetName());
                    rtidbTables.put(castNode.getName_(), rtidbTables.get(dataNode.GetName()));
                }
                continue;
            }
        }
        return rtidbTables;
    }

    public static PhysicalDataProviderNode findDataProviderNode(PhysicalOpNode node) {
        if (node.GetOpType() == PhysicalOpType.kPhysicalOpDataProvider) {
            return PhysicalDataProviderNode.CastFrom(node);
        }
        if (node.GetOpType() == PhysicalOpType.kPhysicalOpSimpleProject) {
            return findDataProviderNode(node.GetProducer(0));
        }
        if (node.GetOpType() == PhysicalOpType.kPhysicalOpRename) {
            return findDataProviderNode(node.GetProducer(0));
        }
        return null;

    }

    public static void dagToList(PhysicalOpNode node, List<PhysicalOpNode> list) {
        PhysicalOpType type = node.GetOpType();
        // 需要针对union node做特殊处理
        if (type.swigValue() == PhysicalOpType.kPhysicalOpRequestUnoin.swigValue()) {
            PhysicalRequestUnionNode castNode = PhysicalRequestUnionNode.CastFrom(node);
            for (int i = 0; i < castNode.window_unions().GetSize(); i++) {
                dagToList(castNode.window_unions().GetKey(i), list);
            }
        }

        for (long i = 0; i < node.GetProducerCnt(); i++) {
            dagToList(node.GetProducer(i), list);
        }
        list.add(node);
    }

    /**
     *
     * @param list
     */
    public static void printDagListInfo(List<PhysicalOpNode> list) {
        for (PhysicalOpNode node : list) {
            System.out.println("dagToList node type = " + node.GetTypeName());
            PhysicalOpType type = node.GetOpType();
            if (type.swigValue() == PhysicalOpType.kPhysicalOpDataProvider.swigValue()) {
                PhysicalDataProviderNode castNode = PhysicalDataProviderNode.CastFrom(node);
                System.out.println("PhysicalDataProviderNode = " + castNode.GetName());
            }
        }
    }

    public static TypeOuterClass.Type getFesqlType(String type) {
        if (type.equalsIgnoreCase("bigint") || type.equalsIgnoreCase("long")) {
            return TypeOuterClass.Type.kInt64;
        }
        if (type.equalsIgnoreCase("smallint") || type.equalsIgnoreCase("small")) {
            return TypeOuterClass.Type.kInt16;
        }
        if (type.equalsIgnoreCase("int")) {
            return TypeOuterClass.Type.kInt32;
        }
        if (type.equalsIgnoreCase("float")) {
            return TypeOuterClass.Type.kFloat;
        }
        if (type.equalsIgnoreCase("double")) {
            return TypeOuterClass.Type.kDouble;
        }
        if (type.equalsIgnoreCase("string")) {
            return TypeOuterClass.Type.kVarchar;
        }
        if (type.equalsIgnoreCase("boolean") || type.equalsIgnoreCase("bool")) {
            return TypeOuterClass.Type.kBool;
        }
        if (type.equalsIgnoreCase("timestamp")) {
            return TypeOuterClass.Type.kTimestamp;
        }
        if (type.equalsIgnoreCase("date")) {
            return TypeOuterClass.Type.kDate;
        }
        return null;
    }

    public static String getDDLType(TypeOuterClass.Type type) {
        if (TypeOuterClass.Type.kInt64 == type) {
            return "bigint";
        }
        if (TypeOuterClass.Type.kInt16 == type) {
            return "smallint";
        }
        if (TypeOuterClass.Type.kInt32 == type) {
            return "int";
        }
        if (TypeOuterClass.Type.kFloat == type) {
            return "float";
        }
        if (TypeOuterClass.Type.kDouble == type) {
            return "double";
        }
        if (TypeOuterClass.Type.kVarchar == type) {
            return "string";
        }
        if (TypeOuterClass.Type.kBool == type) {
            return "bool";
        }
        if (TypeOuterClass.Type.kTimestamp == type) {
            return "timestamp";
        }
        if (TypeOuterClass.Type.kDate == type) {
            return "date";
        }
        return null;
    }

    public static String getRtidbIndexType(TTLType type) {
        if (TTLType.kAbsAndLat == type) {
            return "absandlat";
        }
        if (TTLType.kAbsolute == type) {
            return "absolute";
        }
        if (TTLType.kLatest == type) {
            return "latest";
        }
        if (TTLType.kAbsOrLat == type) {
            return "absorlat";
        }
        return null;

    }

    public static List<TypeOuterClass.TableDef> getTableDefs(String jsonObject) {
        List<TypeOuterClass.TableDef> tableDefs = new ArrayList<>();
//        Gson gson = new Gson();
        JsonParser jsonParser = new JsonParser();
        JsonElement tableJson = jsonParser.parse(jsonObject);
        for (Map.Entry<String, JsonElement> e : tableJson.getAsJsonObject().get("tableInfo").getAsJsonObject().entrySet()) {
            TypeOuterClass.TableDef.Builder table = TypeOuterClass.TableDef.newBuilder();
            table.setName(e.getKey());
            for (JsonElement element : e.getValue().getAsJsonArray()) {
                table.addColumns(TypeOuterClass.ColumnDef.newBuilder()
                        .setName(element.getAsJsonObject().get("name").getAsString())
                        .setIsNotNull(false)
                        .setType(getFesqlType(element.getAsJsonObject().get("type").getAsString())));
            }
            tableDefs.add(table.build());
        }
        return tableDefs;
    }

    public static List<String> addEscapeChar(List<String> list, String singleChar) {
        List<String> newList = new ArrayList<>();
        for (String e : list) {
            String str = String.format("%s%s%s", singleChar, e, singleChar);
            newList.add(str);
        }
        return newList;
    }

    public static String genOutputSchema(String sql, String schema) {
        String tempDB = "temp_" + System.currentTimeMillis();
        TypeOuterClass.Database.Builder db = TypeOuterClass.Database.newBuilder();
        db.setName(tempDB);
        List<TypeOuterClass.TableDef> tables = getTableDefs(schema);
        Map<String, TypeOuterClass.TableDef> tableDefMap = new HashMap<>();
//        tableDefMap.forEach((k, v) -> System.out.printf("xx"));
        for (TypeOuterClass.TableDef e : tables) {
            db.addTables(e);
            tableDefMap.put(e.getName(), e);
        }
        RequestEngine engine = null;
        try {
            engine = new RequestEngine(sql, db.build());
        } catch (UnsupportedFesqlException e) {
            e.printStackTrace();
        }
        PhysicalOpNode plan = engine.getPlan();
        List<Pair<String, String>> schemaPair = new ArrayList<>();

        for (TypeOuterClass.ColumnDef e : plan.GetOutputSchema()) {
            Pair<String, String> field = new Pair<>(e.getName(), getDDLType(e.getType()));
            schemaPair.add(field);
        }

        HashMap<String, List<Pair<String, String>>> feConfig = new HashMap<>();

        feConfig.put(SQLTableName, schemaPair);

        Gson gson = new Gson();
        String jsonConfig = String.format("{\"tableInfo\": %s}", gson.toJson(feConfig));
        System.out.println("=================fe config=================");
        System.out.println(jsonConfig);
        return jsonConfig;

    }


    public static void main(String[] args) {
        //    String schemaPath = "/home/wangzixian/ferrari/idea/docker-code/fesql/java/fesql-common/src/test/resources/ddl/homecredit.json";
        //    String sqlPath = "/home/wangzixian/ferrari/idea/docker-code/fesql/java/fesql-common/src/test/resources/ddl/homecredit.txt";
        String schemaPath = "/home/wangzixian/ferrari/idea/docker-code/fesql/java/fesql-common/src/test/resources/performance/rong_e.json";
        String sqlPath = "/home/wangzixian/ferrari/idea/docker-code/fesql/java/fesql-common/src/test/resources/performance/rong_e.txt";
//        String schemaPath = "/home/wangzixian/ferrari/idea/docker-code/fesql/java/fesql-common/src/test/resources/performance/all_op.json";
//        String sqlPath = "/home/wangzixian/ferrari/idea/docker-code/fesql/java/fesql-common/src/test/resources/performance/all_op.txt";
        File file = new File(schemaPath);
        File sql = new File(sqlPath);
        try {
            genOutputSchema(FileUtils.readFileToString(sql, "UTF-8"), FileUtils.readFileToString(file, "UTF-8"));
//            genDDL(FileUtils.readFileToString(sql, "UTF-8"), FileUtils.readFileToString(file, "UTF-8"));
//            getTableDefs(FileUtils.readFileToString(file, "UTF-8"));
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}

@Data
class RtidbTable {
    String tableName;
    TypeOuterClass.TableDef schema;
    Set<RtidbIndex> indexs = new HashSet<RtidbIndex>();

    // 需要考虑重复的index，并且找到范围最大ttl值
    public void addIndex(RtidbIndex index) {
        boolean flag = true;
        for (RtidbIndex e : indexs) {
            if (index.equals(e)) {
                flag = false;
                if (index.getAtmost() > e.getAtmost()) {
                    e.setAtmost(index.getAtmost());
                }
                if (index.getExpire() > e.getExpire()) {
                    e.setExpire(index.getExpire());
                }
                if (index.getType() == TTLType.kAbsAndLat || index.getType() != e.getType()) {
                    e.setType(TTLType.kAbsAndLat);
                }
                break;
            }
        }
        if (flag) {
            indexs.add(index);
        }
    }

    public String toDDL() {
        StringBuilder str = new StringBuilder();
        str.append("create table ");
        String newTableName = String.format("`%s`", tableName);
        str.append(newTableName);
        str.append("(\n");
        for (int i = 0; i < schema.getColumnsCount(); i++) {
            String name = String.format("`%s`", schema.getColumns(i).getName());
            str.append(name);
            str.append(" ");
            str.append(DDLEngine.getDDLType(schema.getColumns(i).getType()));
            str.append(",\n");
        }
        List<String> indexsList = new ArrayList<>();
        for (RtidbIndex e : indexs) {
            indexsList.add(e.toIndexString());
        }
        str.append(StringUtils.join(indexsList, ",\n"));
        str.append("\n)");
        return str.toString();
    }
}

enum TTLType {
    kAbsolute,
    kLatest,
    kAbsOrLat,
    kAbsAndLat
}

@Data
class RtidbIndex {
    private List<String> keys = new ArrayList<>();
    private String ts = "";
    // 因为fesql支持任意范围的窗口，所以需要kAbsAndLat这个类型。确保窗口中本该有数据，而没有被淘汰出去
    private TTLType type = TTLType.kAbsAndLat;
    // 映射到ritdb是最多保留多少条数据，不是最少
    private long atmost = 0;
    // 毫秒单位
    private long expire = 0;
    public String toIndexString() {
        List<String> newKeys = DDLEngine.addEscapeChar(keys, "`");
        String key = StringUtils.join(newKeys, ",");
        String ttlType = DDLEngine.getRtidbIndexType(type);
        String index = "";
        if (ts.equals("")) {
            index = String.format("index(key=(%s), ttl=%s, ttl_type=%s)", key, getTTL(), ttlType);
        } else {
            index = String.format("index(key=(%s), ts=`%s`, ttl=%s, ttl_type=%s)", key, ts, getTTL(), ttlType);
        }
        return index;
    }

    public String getTTL() {
        if (TTLType.kAbsAndLat == type) {
            long expireStr = expire / (60 * 1000);
            return "(" + expireStr + "m, " + atmost + ")";
        }
        if (TTLType.kAbsolute == type) {
            long expireStr = expire / (60 * 1000);
            return expireStr + "m";
        }
        if (TTLType.kLatest == type) {
            return String.valueOf(atmost);
        }
        if (TTLType.kAbsOrLat == type) {
            long expireStr = expire / (60 * 1000);
            return "(" + expireStr + "m, " + atmost + ")";
        }
        return null;
    }
    //    @Override
    public boolean equals(RtidbIndex e) {
        return  this.getType() == e.getType() && this.getKeys().equals(e.getKeys()) && this.ts.equals(e.getTs());
    }
}

@Data
class Pair<K, V> {
    private K name;
    private V type;
    public Pair(K k, V v) {
        name = k;
        type = v;
    }
}