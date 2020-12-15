package com._4paradigm.sql.jmh;

import com._4paradigm.fesql.sqlcase.model.ExpectDesc;
import com._4paradigm.fesql.sqlcase.model.InputDesc;
import com._4paradigm.fesql.sqlcase.model.SQLCase;
import com._4paradigm.fesql.sqlcase.model.Table;
import com._4paradigm.sql.sdk.SqlExecutor;
import com._4paradigm.sql.tools.Util;
import com._4paradigm.sql.tools.Relation;
import com._4paradigm.sql.tools.TableInfo;
import com.google.common.collect.Lists;
import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.runner.RunnerException;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;
import java.sql.*;
import java.sql.Date;
import java.util.*;
import java.util.concurrent.TimeUnit;
import org.yaml.snakeyaml.Yaml;

@BenchmarkMode(Mode.SampleTime)
@OutputTimeUnit(TimeUnit.MICROSECONDS)
@State(Scope.Benchmark)
@Threads(1)
@Fork(value = 1, jvmArgs = {"-Xms32G", "-Xmx32G"})
@Warmup(iterations = 1)

public class FESQLFZBenchmark {
    private SqlExecutor executor;
    private String db;
    private String ddlUrl = "http://172.27.128.37:8999/fz_ddl/batch_request100680.txt.ddl.txt";
    private String scriptUrl = "http://172.27.128.37:8999/fz_ddl/batch_request100680.txt";
    private String relationUrl = "http://172.27.128.37:8999/fz_ddl/batch_request100680.relation.txt";
    private Boolean enableOutputYamlCase = false;
    SQLCase sqlCase = new SQLCase();
    private int pkNum = 1;
    //@Param({"500", "1000", "2000"})
    private int windowNum = 2000;
    private Map<String, TableInfo> tableMap;
    private String script;
    private String mainTable;
    List<Map<String, String>> mainTableValue;
    int pkBase = 1000000;
    public FESQLFZBenchmark() {
        this(false, false);
    }
    public FESQLFZBenchmark(boolean enableDebug , boolean enableOutputYamlCase) {
        executor = BenchmarkConfig.GetSqlExecutor(enableDebug);
        db = "db" + System.nanoTime();
        tableMap = new HashMap<>();
        mainTableValue = new ArrayList<>();
        this.enableOutputYamlCase = enableOutputYamlCase;
    }

    public void setWindowNum(int windowNum) {
        this.windowNum = windowNum;
    }

    public void init() {
        String rawScript = Util.getContent(scriptUrl);
        script = rawScript.trim().replace("\n", " ");
        Relation relation = new Relation(Util.getContent(relationUrl));
        mainTable = relation.getMainTable();
        String ddl = Util.getContent(ddlUrl);
        String[] arr = ddl.split(";");
        for (String item : arr) {
            item = item.trim().replace("\n", "");
            if (item.isEmpty()) {
                continue;
            }
            TableInfo table = new TableInfo(item, relation);
            tableMap.put(table.getName(), table);
        }
        System.out.println(db);
    }

    public void putData() {
        TableInfo mainTableInfo = tableMap.get(mainTable);
        putTableData(mainTableInfo);
        for (Map.Entry<String, TableInfo> entry : tableMap.entrySet()) {
            if (entry.getKey().equals(mainTable)) {
                continue;
            }
            putTableData(entry.getValue());
        }
    }

    private void putTableData(TableInfo table) {
        List<String> inserts = Lists.newArrayList();
        boolean isMainTable = false;
        if (table.getName().equals(mainTable)) {
            isMainTable = true;
        }
        if (!isMainTable && mainTableValue.size() != pkNum) {
            return;
        }
        List<String> schema = table.getSchema();
        Set<Integer> index = table.getIndex();
        Set<Integer> tsIndex = table.getTsIndex();
        Map<Integer, String> relation = table.getColRelation();

        Map<String, String> valueMap;
        for (int i = 0; i < pkNum; i++) {
            long ts = System.currentTimeMillis();
            if (isMainTable) {
                valueMap = new HashMap<>();
            } else {
                valueMap = mainTableValue.get(i);
            }
            for (int tsCnt = 0; tsCnt < windowNum; tsCnt++) {
                StringBuilder builder = new StringBuilder();
                builder.append("insert into ");
                builder.append(table.getName());
                builder.append(" values(");
                for (int pos = 0; pos < schema.size(); pos++) {
                    if (pos > 0) {
                        builder.append(", ");
                    }
                    String type = schema.get(pos);
                    if (!isMainTable && index.contains(pos)) {
                        if (type.equals("string")) {
                            builder.append("'");
                        }
                        builder.append(valueMap.get(relation.get(pos)));
                        if (type.equals("string")) {
                            builder.append("'");
                        }
                        continue;
                    }
                    if (type.equals("string")) {
                        builder.append("'");
                        builder.append("col");
                        builder.append(pos);
                        if (index.contains(pos)) {
                            String fieldName = table.getSchemaPosName().get(pos);
                            if (!valueMap.containsKey(fieldName)) {
                                valueMap.put(fieldName, "col" + pos);
                            }
                        }
                        builder.append("'");
                    } else if (type.equals("float")) {
                        builder.append(1.3);
                    } else if (type.equals("double")) {
                        builder.append("1.4");
                    } else if (type.equals("bigint") || type.equals("timestamp") || type.equals("int")) {
                        if (index.contains(pos)) {
                            builder.append(pkBase + i);
                            String fieldName = table.getSchemaPosName().get(pos);
                            if (!valueMap.containsKey(fieldName)) {
                                valueMap.put(fieldName, String.valueOf(pkBase + i));
                            }
                        } else if (tsIndex.contains(pos)) {
                            builder.append(ts - tsCnt);
                        } else {
                            if (type.equals("timestamp")) {
                                builder.append(ts);
                            } else {
                                builder.append(pos);
                            }
                        }
                    } else if (type.equals("bool")) {
                        builder.append(true);
                    } else if (type.equals("date")) {
                        builder.append("'2020-11-27'");
                    } else {
                        System.out.println("invalid type");
                    }
                }
                builder.append(");");
                String exeSql = builder.toString();
                //System.out.println(exeSql);
                inserts.add(exeSql);
            }
            if (isMainTable) {
                mainTableValue.add(valueMap);
            }
        }
        for(String exeSql: inserts) {
            executor.executeInsert(db, exeSql);
        }
        if (enableOutputYamlCase) {
            InputDesc inputDesc = new InputDesc();
            inputDesc.setName(table.getName());
            inputDesc.setCreate(table.getDDL());
            inputDesc.setInserts(inserts);
            sqlCase.getInputs().add(inputDesc);
        }
    }

    private PreparedStatement getPreparedStatement() throws SQLException {
        PreparedStatement requestPs = executor.getRequestPreparedStmt(db, script);
        ResultSetMetaData metaData = requestPs.getMetaData();
        TableInfo table = tableMap.get(mainTable);
        if (table.getSchema().size() != metaData.getColumnCount()) {
            return null;
        }
        for (int i = 0; i < metaData.getColumnCount(); i++) {
            int columnType = metaData.getColumnType(i + 1);
            if (columnType == Types.VARCHAR) {
                requestPs.setString(i + 1, "col" + String.valueOf(i));
            } else if (columnType == Types.DOUBLE) {
                requestPs.setDouble(i + 1, 1.4d);
            } else if (columnType == Types.FLOAT) {
                requestPs.setFloat(i + 1, 1.3f);
            } else if (columnType == Types.INTEGER) {
                if (table.getIndex().contains(i)) {
                    requestPs.setInt(i + 1, pkBase + i);
                } else {
                    requestPs.setInt(i + 1, i);
                }
            } else if (columnType == Types.BIGINT) {
                if (table.getIndex().contains(i)) {
                    requestPs.setLong(i + 1, pkBase + i);
                } else {
                    requestPs.setLong(i + 1, i);
                }
            } else if (columnType == Types.TIMESTAMP) {
                requestPs.setTimestamp(i + 1, new Timestamp(System.currentTimeMillis()));
            } else if (columnType == Types.DATE) {
                requestPs.setDate(i + 1, new Date(System.currentTimeMillis()));
            }
        }
        return  requestPs;
    }

    @Setup
    public void setup() throws SQLException {
        init();
        if (!executor.createDB(db)) {
            return;
        }
        for (TableInfo table : tableMap.values()) {
            //System.out.println(table.getDDL());
            if (!executor.executeDDL(db, table.getDDL())) {
                return;
            }
        }
        putData();
        if (enableOutputYamlCase) {
            sqlCase.setSql(script);
            sqlCase.setExpect(new ExpectDesc());
        }
    }
    public Boolean outputSQLCase() {
        if (sqlCase == null) {
            return false;
        }
        Yaml yaml = new Yaml();
        System.out.println(yaml.dump(sqlCase));
        return true;
    }

    @TearDown
    public void teardown() throws SQLException {
        /*for (String name : tableMap.keySet()) {
            executor.executeDDL(db, "drop table " + name + ";");
        }
        executor.dropDB(db);*/
    }

    @Benchmark
    public void execSQL() {
        try {
            PreparedStatement ps = getPreparedStatement();
            ResultSet resultSet = ps.executeQuery();
            /*resultSet.next();
            ResultSetMetaData metaData = resultSet.getMetaData();
            Map<String, String> val = new HashMap<>();
            int stringNum = 0;
            for (int i = 0; i < metaData.getColumnCount(); i++) {
                String columnName = metaData.getColumnName(i + 1);
                System.out.println(columnName + ":" + String.valueOf(i));
                int columnType = metaData.getColumnType(i + 1);
                if (columnType == Types.VARCHAR) {
                    val.put(columnName, String.valueOf(resultSet.getString(i + 1)));
                    stringNum++;
                    System.out.println(columnName + ":" + String.valueOf(i) + ":" + resultSet.getString(i+ 1));

                } else if (columnType == Types.DOUBLE) {
                    val.put(columnName, String.valueOf(resultSet.getDouble(i + 1)));
                } else if (columnType == Types.INTEGER) {
                    val.put(columnName, String.valueOf(resultSet.getInt(i + 1)));
                } else if (columnType == Types.BIGINT) {
                    val.put(columnName, String.valueOf(resultSet.getLong(i + 1)));
                } else if (columnType == Types.TIMESTAMP) {
                    val.put(columnName, String.valueOf(resultSet.getTimestamp(i + 1)));
                } else if (columnType == Types.DATE) {
                    val.put(columnName, String.valueOf(resultSet.getDate(i+ 1)));
                }
            }
            System.out.println("string num" + stringNum);
            ps.close();*/
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public Map<String, String> execSQLTest() {
        try {
            PreparedStatement ps = getPreparedStatement();
            ResultSet resultSet = ps.executeQuery();
            resultSet.next();
            ResultSetMetaData metaData = resultSet.getMetaData();
            Map<String, String> val = new HashMap<>();
            for (int i = 0; i < metaData.getColumnCount(); i++) {
                String columnName = metaData.getColumnName(i + 1);
                int columnType = metaData.getColumnType(i + 1);
                if (columnType == Types.VARCHAR) {
                    val.put(columnName, String.valueOf(resultSet.getString(i + 1)));
                } else if (columnType == Types.DOUBLE) {
                    val.put(columnName, String.valueOf(resultSet.getDouble(i + 1)));
                } else if (columnType == Types.INTEGER) {
                    val.put(columnName, String.valueOf(resultSet.getInt(i + 1)));
                } else if (columnType == Types.BIGINT) {
                    val.put(columnName, String.valueOf(resultSet.getLong(i + 1)));
                } else if (columnType == Types.TIMESTAMP) {
                    val.put(columnName, String.valueOf(resultSet.getTimestamp(i + 1)));
                }
            }
            ps.close();
            return val;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    public static void main(String[] args) throws RunnerException {
      /*FESQLFZBenchmark ben = new FESQLFZBenchmark();
      try {
          ben.setup();
          ben.execSQL();
          ben.teardown();
      } catch (Exception e) {
          e.printStackTrace();
      }*/
        Options opt = new OptionsBuilder()
                .include(FESQLFZBenchmark.class.getSimpleName())
                .forks(1)
                .build();
        new Runner(opt).run();
    }
}
