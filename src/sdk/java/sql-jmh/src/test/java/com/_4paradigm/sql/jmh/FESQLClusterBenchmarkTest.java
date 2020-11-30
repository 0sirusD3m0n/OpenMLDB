package com._4paradigm.sql.jmh;

import org.junit.Assert;
import org.junit.Ignore;
import org.junit.Test;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.RunnerException;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.sql.SQLException;
import java.util.Map;

public class FESQLClusterBenchmarkTest {
    private static Logger logger = LoggerFactory.getLogger(FESQLClusterBenchmark.class);
    @Test
    public void windowLastJoinTest() throws SQLException {
        FESQLClusterBenchmark benchmark = new FESQLClusterBenchmark(true);
        benchmark.setWindowNum(1000);
        benchmark.setup();
        int loops = 10;
        for (int i = 0; i < loops; i++) {
            Map<String, String> result = benchmark.execSQLTest();
            Assert.assertNotNull(result);
            Assert.assertTrue(result.size() > 0);
            System.out.println("previous_application_SK_ID_CURR_time_0s_32d_CNT: " + result.get("previous_application_SK_ID_CURR_time_0s_32d_CNT"));
            System.out.println("POS_CASH_balance_SK_ID_CURR_time_0s_32d_CNT: " + result.get("POS_CASH_balance_SK_ID_CURR_time_0s_32d_CNT"));
            System.out.println("installments_payments_SK_ID_CURR_time_0s_32d_CNT: " + result.get("installments_payments_SK_ID_CURR_time_0s_32d_CNT"));
            System.out.println("bureau_balance_SK_ID_CURR_time_0s_32d_CNT: " + result.get("bureau_balance_SK_ID_CURR_time_0s_32d_CNT"));
            System.out.println("credit_card_balance_SK_ID_CURR_time_0s_32d_CNT: " + result.get("credit_card_balance_SK_ID_CURR_time_0s_32d_CNT"));
            System.out.println("bureau_SK_ID_CURR_time_0s_32d_CNT: " + result.get("bureau_SK_ID_CURR_time_0s_32d_CNT"));
            System.out.println("----------------------------");
        }
        benchmark.teardown();
    }

    @Test
    @Ignore
    public void benchmark() throws RunnerException {
        Options opt = new OptionsBuilder()
                .include(FESQLClusterBenchmark.class.getSimpleName())
                .forks(1)
                .build();
        new Runner(opt).run();
    }
}
