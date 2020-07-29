package com._4paradigm.fesql.flink.batch;

import java.util.List;
import org.apache.flink.api.java.ExecutionEnvironment;
import org.apache.flink.formats.parquet.ParquetTableSource;
import org.apache.flink.table.api.Table;
import org.apache.flink.table.api.bridge.java.BatchTableEnvironment;
import org.apache.flink.types.Row;
import org.apache.parquet.schema.MessageType;
import org.junit.Test;
import com._4paradigm.fesql.flink.common.ParquetHelper;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

public class TestFesqlBatchTableEnvironment {

    @Test
    public void testUseFesqlBatchTableEnvironment() throws Exception {
        ExecutionEnvironment env = ExecutionEnvironment.getExecutionEnvironment();
        BatchTableEnvironment originEnv = BatchTableEnvironment.create(env);
        FesqlBatchTableEnvironment tEnv = new FesqlBatchTableEnvironment(originEnv);

        // Init input source
        String parquetFilePath = this.getClass().getResource("/int_int_int.parquet").getPath();
        MessageType messageType = ParquetHelper.readParquetSchema(parquetFilePath);
        ParquetTableSource parquetSrc = ParquetTableSource.builder()
                .path(parquetFilePath)
                .forParquetSchema(messageType)
                .build();
        tEnv.registerTableSource("t1", parquetSrc);

        // Run sql
        String sqlText = "select vendor_id + 1000, passenger_count * 10, trip_duration - 10 from t1";
        Table table = tEnv.sqlQuery(sqlText);

        // Check result
        List<Row> fesqlResult = tEnv.toDataSet(table, Row.class).collect();
        List<Row> flinksqlResult = originEnv.toDataSet(originEnv.sqlQuery(sqlText), Row.class).collect();
        assertNotEquals(fesqlResult.size(), 0);

        for (int i=0; i < fesqlResult.size(); ++i) {
            Row fesqlRow = fesqlResult.get(i);
            Row flinksqlRow = flinksqlResult.get(i);

            for (int j=0; j < fesqlRow.getArity(); ++j) {
                assertEquals(fesqlRow.getField(j), flinksqlRow.getField(j));
            }
        }
    }

}
