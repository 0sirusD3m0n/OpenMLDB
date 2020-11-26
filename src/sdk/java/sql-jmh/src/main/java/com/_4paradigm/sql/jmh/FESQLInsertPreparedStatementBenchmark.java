package com._4paradigm.sql.jmh;

import com._4paradigm.sql.sdk.SdkOption;
import com._4paradigm.sql.sdk.SqlExecutor;
import com._4paradigm.sql.sdk.impl.SqlClusterExecutor;
import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.RunnerException;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;

import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.sql.Timestamp;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

@BenchmarkMode(Mode.SampleTime)
@OutputTimeUnit(TimeUnit.SECONDS)
@State(Scope.Benchmark)
@Threads(1)
@Fork(value = 1, jvmArgs = {"-Xms4G", "-Xmx4G"})
@Warmup(iterations = 1)
public class FESQLInsertPreparedStatementBenchmark {
    private AtomicLong counter = new AtomicLong(0l);
    private SqlExecutor executor;
    private SdkOption option;
    private String db = "db_insert_benchmark" + System.nanoTime();
    private int recordSize = 10000;
    private String ddl100;
    private String ddl100Insert;

    private String ddl200;
    private String ddl200Insert;

    private String ddl500;
    private String ddl500Insert;

    public FESQLInsertPreparedStatementBenchmark() {
        SdkOption sdkOption = new SdkOption();
        sdkOption.setSessionTimeout(30000);
        sdkOption.setZkCluster(BenchmarkConfig.ZK_CLUSTER);
        sdkOption.setZkPath(BenchmarkConfig.ZK_PATH);
        this.option = sdkOption;
        try {
            executor = new SqlClusterExecutor(option);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Setup
    public void setup() throws SQLException {
        boolean setupOk = executor.createDB(db);
        if (!setupOk) {
            return;
        }
        StringBuilder ddl100Builder = new StringBuilder();
        StringBuilder ddl100InsertBuilder = new StringBuilder();
        ddl100InsertBuilder.append("insert into ddl100 values(");
        ddl100Builder.append("create table ddl100(");
        for (int i = 0;  i < 99; i++) {
            if (i > 0) {
                ddl100Builder.append(",");
                ddl100InsertBuilder.append(",");
            }
            ddl100Builder.append("col" + String.valueOf(i) + " string");
            ddl100InsertBuilder.append("?");
        }
        ddl100Builder.append(", col99 timestamp, index(key=col98, ts=col99));");
        ddl100InsertBuilder.append(", ?);");
        ddl100 = ddl100Builder.toString();
        ddl100Insert = ddl100InsertBuilder.toString();
        setupOk = executor.executeDDL(db, ddl100);
        if (!setupOk) {
            return;
        }
        {
            StringBuilder ddl200Builder = new StringBuilder();
            StringBuilder ddl200InsertBuilder = new StringBuilder();
            ddl200InsertBuilder.append("insert into ddl200 values(");
            ddl200Builder.append("create table ddl200(");
            for (int i = 0;  i < 199; i++) {
                if (i > 0) {
                    ddl200Builder.append(",");
                    ddl200InsertBuilder.append(",");
                }
                ddl200Builder.append("col" + String.valueOf(i) + " string");
                ddl200InsertBuilder.append("?");
            }
            ddl200Builder.append(", col199 timestamp, index(key=col198, ts=col199));");
            ddl200InsertBuilder.append(", ?);");
            ddl200 = ddl200Builder.toString();
            ddl200Insert = ddl200InsertBuilder.toString();
            setupOk = executor.executeDDL(db, ddl200);
            if (!setupOk) {
                return;
            }
        }
        {
            StringBuilder ddl500Builder = new StringBuilder();
            StringBuilder ddl500InsertBuilder = new StringBuilder();
            ddl500InsertBuilder.append("insert into ddl500 values(");
            ddl500Builder.append("create table ddl500(");
            for (int i = 0;  i < 499; i++) {
                if (i > 0) {
                    ddl500Builder.append(",");
                    ddl500InsertBuilder.append(",");
                }
                ddl500Builder.append("col" + String.valueOf(i) + " string");
                ddl500InsertBuilder.append("?");
            }
            ddl500Builder.append(", col499 timestamp, index(key=col498, ts=col499));");
            ddl500InsertBuilder.append(", ?);");
            ddl500 = ddl500Builder.toString();
            ddl500Insert = ddl500InsertBuilder.toString();
            setupOk = executor.executeDDL(db, ddl500);
            if (!setupOk) {
                return;
            }
        }
        try {
            Thread.sleep(2000);
        } catch (Exception e) {

        }
    }

    @Benchmark
    public void insert100Bm() {
        String key = "100_"+ String.valueOf(counter.incrementAndGet());
        PreparedStatement impl = executor.getInsertPreparedStmt(db, ddl100Insert);
        try {
            for (int i = 0; i < 98; i++) {
                impl.setString(i+1, "value0");
            }
            impl.setString(99, key);
            impl.setTimestamp(100, new Timestamp(System.currentTimeMillis()));
            impl.execute();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Benchmark
    public void insert500Bm() {
        String key = "500_"+ String.valueOf(counter.incrementAndGet());
        PreparedStatement impl = executor.getInsertPreparedStmt(db, ddl500Insert);
        try {
            for (int i = 0; i < 498; i++) {
                impl.setString(i+1, "value0");
            }
            impl.setString(499, key);
            impl.setTimestamp(500, new Timestamp(System.currentTimeMillis()));
            impl.execute();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    @Benchmark
    public void insert200Bm() {
        String key = "200_"+ String.valueOf(counter.incrementAndGet());
        PreparedStatement impl = executor.getInsertPreparedStmt(db, ddl200Insert);
        try {
            for (int i = 0; i < 198; i++) {
                impl.setString(i+1, "value0");
            }
            impl.setString(199, key);
            impl.setTimestamp(200, new Timestamp(System.currentTimeMillis()));
            impl.execute();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    public static void main(String[] args) throws RunnerException {
       Options opt = new OptionsBuilder()
                .include(FESQLInsertPreparedStatementBenchmark.class.getSimpleName())
                .forks(1)
                .build();
        new Runner(opt).run();
    }
}
