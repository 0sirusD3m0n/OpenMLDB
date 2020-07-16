package com._4paradigm.fesql_auto_test.executor;

import com._4paradigm.fesql.sqlcase.model.SQLCase;
import com._4paradigm.fesql_auto_test.checker.Checker;
import com._4paradigm.fesql_auto_test.checker.CheckerStrategy;
import com._4paradigm.fesql_auto_test.entity.FesqlResult;
import com._4paradigm.sql.sdk.SqlExecutor;
import lombok.extern.slf4j.Slf4j;
import org.testng.Assert;

import java.util.List;

/**
 * @author zhaowei
 * @date 2020/6/15 11:23 AM
 */
@Slf4j
public abstract class BaseExecutor {
    protected SQLCase fesqlCase;
    protected SqlExecutor executor;

    public BaseExecutor(SqlExecutor executor, SQLCase fesqlCase) {
        this.executor = executor;
        this.fesqlCase = fesqlCase;
    }

    public void run() {
        log.info(fesqlCase.getDesc() + " Begin!");
        if (null == fesqlCase) {
            Assert.fail("executor run with null case");
            return;
        }
        try {
            prepare();
            FesqlResult fesqlResult = execute();
            check(fesqlResult);
            tearDown();
        } catch (Exception e) {
            e.printStackTrace();
            Assert.fail("executor run with exception");
        }
    }

    protected abstract void prepare() throws Exception;

    protected abstract FesqlResult execute() throws Exception;

    protected FesqlResult after() {
        return null;
    }

    protected void check(FesqlResult fesqlResult) throws Exception {
        List<Checker> strategyList = CheckerStrategy.build(fesqlCase, fesqlResult);
        for (Checker checker : strategyList) {
            checker.check();
        }
    }

    protected void tearDown() {
    }
}
