package com._4paradigm.fesql_auto_test.executor;

import com._4paradigm.fesql.sqlcase.model.InputDesc;
import com._4paradigm.fesql.sqlcase.model.SQLCase;
import com._4paradigm.fesql_auto_test.entity.FesqlResult;
import com._4paradigm.fesql_auto_test.util.FesqlUtil;
import com._4paradigm.sql.sdk.SqlExecutor;
import lombok.extern.slf4j.Slf4j;
import org.apache.commons.collections.CollectionUtils;

import java.util.ArrayList;
import java.util.List;

@Slf4j
public class RequestQuerySQLExecutor extends SQLExecutor {

    private boolean isBatchRequest;

    public RequestQuerySQLExecutor(SqlExecutor executor, SQLCase fesqlCase, boolean isBatchRequest) {
        super(executor, fesqlCase);
        this.isBatchRequest = isBatchRequest;
    }

    @Override
    protected void prepare() throws Exception {
        boolean dbOk = executor.createDB(dbName);
        log.info("create db:{},{}", dbName, dbOk);
        boolean useFirstInputAsRequests = !isBatchRequest;
        FesqlResult res = FesqlUtil.createAndInsert(executor, dbName, fesqlCase.getInputs(), useFirstInputAsRequests);
        if (!res.isOk()) {
            throw new RuntimeException("fail to run SQLExecutor: prepare fail");
        }
        for (InputDesc inputDesc : fesqlCase.getInputs()) {
            tableNames.add(inputDesc.getName());
        }
    }

    @Override
    public void run() {
        if (null != fesqlCase.getMode() && fesqlCase.getMode().contains("request-unsupport")) {
            log.info("skip case in request mode: {}", fesqlCase.getDesc());
            return;
        }
        if (null != fesqlCase.getMode() && fesqlCase.getMode().contains("rtidb-unsupport")) {
            log.info("skip case in rtidb mode: {}", fesqlCase.getDesc());
            return;
        }
        if (null != fesqlCase.getMode() && fesqlCase.getMode().contains("rtidb-request-unsupport")) {
            log.info("skip case in rtidb request mode: {}", fesqlCase.getDesc());
            return;
        }
        process();
    }

    @Override
    protected FesqlResult execute() throws Exception {
        FesqlResult fesqlResult = null;
        List<String> sqls = fesqlCase.getSqls();
        if (sqls != null && sqls.size() > 0) {
            for (String sql : sqls) {
                log.info("sql:{}", sql);
                sql = FesqlUtil.formatSql(sql, tableNames);
                fesqlResult = FesqlUtil.sql(executor, dbName, sql);
            }
        }
        String sql = fesqlCase.getSql();
        if (sql != null && sql.length() > 0) {
            if (fesqlCase.getInputs().isEmpty() || CollectionUtils.isEmpty(fesqlCase.getInputs().get(0).getRows())) {
                log.error("fail to execute in request query sql executor: sql case inputs is empty");
                return null;
            }
            log.info("sql:{}", sql);
            sql = FesqlUtil.formatSql(sql, tableNames);
            if (isBatchRequest) {
                InputDesc batchRequest = fesqlCase.getBatch_request();
                if (batchRequest == null) {
                    log.error("No batch request provided in case");
                    return null;
                }
                List<Integer> commonColumnIndices = new ArrayList<>();
                if (batchRequest.getCommon_column_indices() != null) {
                    for (String str : batchRequest.getCommon_column_indices()) {
                        if (str != null) {
                            commonColumnIndices.add(Integer.parseInt(str));
                        }
                    }
                }

                fesqlResult = FesqlUtil.sqlBatchRequestMode(
                        executor, dbName, sql, batchRequest, commonColumnIndices);
            } else {
                InputDesc input = fesqlCase.getInputs().get(0);
                fesqlResult = FesqlUtil.sqlRequestMode(executor, dbName, sql, input);
            }
        }
        return fesqlResult;
    }
}
