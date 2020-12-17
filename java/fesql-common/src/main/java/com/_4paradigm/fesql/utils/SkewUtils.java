package com._4paradigm.fesql.utils;

import org.apache.commons.lang3.StringUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * @Author wangzixian
 * @Description TODO
 * @Date 2020/12/2 19:15
 **/
public class SkewUtils {

    public static String genPercentileSql(String table1, int quantile, List<String> keys, String ts, String cnt) {
        StringBuffer sql = new StringBuffer();
        sql.append("select \n");
        for (String e : keys) {
            sql.append(e + ",\n");
        }
//        count(employee_name, department) as key_cnt,
        sql.append(String.format("count(%s) as %s,\n", StringUtils.join(keys, ","), cnt));
        sql.append(String.format("min(%s) as min_%s,\n", ts, ts));
        sql.append(String.format("max(%s) as max_%s,\n", ts, ts));
        sql.append(String.format("mean(%s) as mean_%s,\n", ts, ts));
        sql.append(String.format("sum(%s) as sum_%s,\n", ts, ts));
        double factor = 1.0 / new Double(quantile);
        for (int i = 0; i < quantile; i++) {
            double v = i * factor;
            sql.append(String.format("percentile_approx(%s, %s) as percentile_%s,\n", ts, v, i));
        }
        sql.append(String.format("percentile_approx(%s, 1) as percentile_%s\n", ts, quantile));
        sql.append(String.format("from \n%s\ngroup by ", table1));
        sql.append(StringUtils.join(keys, " , "));
        sql.append(";");
//        System.out.println(sql);
        return sql.toString();
    }

    /**
     *
     * @param table1
     * @param table2
     * @param quantile
     * @param schemas
     * @param keysMap
     * @param ts
     * @param tag1 skewTag
     * @param tag2 skewPosition
     * @param tag3 skewCntName
     * @return
     */
    public static String genPercentileTagSql(String table1, String table2, int quantile, List<String> schemas, Map<String, String> keysMap, String ts, String tag1, String tag2, String tag3) {
        StringBuffer sql = new StringBuffer();
        sql.append("select \n");
        for (String e : schemas) {
            sql.append(table1 + "." + e + ",");
        }

        sql.append(caseWhenTag(table1, table2, ts, quantile, tag1, tag3));
        sql.append(",");
        sql.append(caseWhenTag(table1, table2, ts, quantile, tag2, tag3));


        sql.append(String.format("from %s left join %s on ", table1, table2));
        List<String> conditions = new ArrayList<>();
        for (Map.Entry<String, String> e : keysMap.entrySet()) {
            String cond = String.format("%s.%s = %s.%s", table1, e.getKey(), table2, e.getValue());
            conditions.add(cond);
        }
        sql.append(StringUtils.join(conditions, " and "));
        sql.append(";");
        return sql.toString();
    }

    /**
     * ?shu
     * @paraable1
     * @param ts
     * @param quantile
     * @param output
     * @return
     */
    public static String caseWhenTag(String table1, String table2, String ts, int quantile, String output, String con1) {
        StringBuffer sql = new StringBuffer();
        sql.append("\ncase\n");
        sql.append(String.format("when %s.%s < %s then 1\n", table2, con1, quantile));
        for (int i = 0; i < quantile; i++) {
            if (i == 0) {
                sql.append(String.format("when %s.%s <= percentile_%s then %d\n", table1, ts, i, quantile - i));
            }

            sql.append(String.format("when %s.%s > percentile_%s and %s.%s <= percentile_%d then %d\n", table1, ts, i, table1, ts, i+1, quantile - i));
            if (i == quantile) {
                sql.append(String.format("when %s.%s > percentile_%s then %d\n", table1, ts, i+1, quantile - i));
            }
        }
        sql.append("end as " + output + "\n");
        return sql.toString();
    }

    public static void main(String[] args) {
        String table1 = "mainTable";
        String table2 = "info_table";
        String ts = "bonus";
        Map<String, String> keys = new HashMap<>();
        keys.put("employee_name", "employee_name");
        keys.put("department", "department");
        List<String> schemas = new ArrayList<String>(){
            {
                add("employee_name");
                add("department");
                add("state");
                add("salary");
                add("age");
                add("bonus");
            }
        };
        System.out.println(genPercentileTagSql(table1, table2, 4, schemas, keys, ts, "tag", "pos", "z"));
    }
}

