package com._4paradigm.fesql.stream;

import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.table.api.DataTypes;
import org.apache.flink.table.api.EnvironmentSettings;
import org.apache.flink.table.api.Table;
import org.apache.flink.table.api.bridge.java.StreamTableEnvironment;
import org.apache.flink.table.sources.CsvTableSource;
import org.apache.flink.types.Row;
import org.junit.Test;


public class TestFesqlStreamTableEnvironment {

    @Test
    public void testUseFesqlBatchTableEnvironment() throws Exception {
        StreamExecutionEnvironment bsEnv = StreamExecutionEnvironment.getExecutionEnvironment();
        EnvironmentSettings bsSettings = EnvironmentSettings.newInstance().useBlinkPlanner().inStreamingMode().build();
        StreamTableEnvironment sEnvOrigin = StreamTableEnvironment.create(bsEnv, bsSettings);
        FesqlStreamTableEnvironment sEnv = FesqlStreamTableEnvironment.create(sEnvOrigin);

        // Input source
        String csvFilePath = this.getClass().getResource("/int_int_int.csv").getPath();
        CsvTableSource csvSrc = CsvTableSource.builder()
                .path(csvFilePath)
                .field("vendor_id", DataTypes.INT())
                .field("passenger_count", DataTypes.INT())
                .field("trip_duration", DataTypes.INT())
                .build();
        sEnv.registerTableSource("t1", csvSrc);

        // Run sql
        String sqlText = "select vendor_id + 1000, passenger_count * 10, trip_duration - 10 from t1";
        Table table = sEnv.sqlQuery(sqlText);

        // Check running without exception
        sEnv.toAppendStream(table, Row.class).print();
        bsEnv.execute();

    }

}
