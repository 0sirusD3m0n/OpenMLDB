package com._4paradigm.sql.sdk.impl;

import com._4paradigm.sql.*;
import com._4paradigm.sql.jdbc.SQLResultSet;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.sql.*;
import java.sql.ResultSet;
import java.util.List;


public class BatchRequestPreparedStatementImpl extends RequestPreparedStatementImpl {
    private static final Logger logger = LoggerFactory.getLogger(BatchRequestPreparedStatementImpl.class);

    private ColumnIndicesSet commonColumnIndices;
    private SQLRequestRowBatch currentRowBatch;

    public BatchRequestPreparedStatementImpl(String db, String sql,
                                             SQLRouter router,
                                             List<Integer> commonColumnIdxList) throws SQLException {
        super(db, sql, router);
        this.commonColumnIndices = new ColumnIndicesSet(currentSchema);
        for (Integer idx : commonColumnIdxList) {
            if (idx != null) {
                this.commonColumnIndices.AddCommonColumnIdx(idx);
            }
        }
        this.currentRowBatch = new SQLRequestRowBatch(currentSchema, commonColumnIndices);
    }

    @Override
    public SQLResultSet executeQuery() throws SQLException {
        checkClosed();
        Status status = new Status();
        com._4paradigm.sql.ResultSet resultSet = router.ExecuteSQLBatchRequest(
                db, currentSql, currentRowBatch, status);
        if (resultSet == null) {
            throw new SQLException("execute sql fail: " + status.getMsg());
        }
        SQLResultSet rs = new SQLResultSet(resultSet);
        if (closeOnComplete) {
            closed = true;
        }
        return rs;
    }

    @Override
    public void addBatch() throws SQLException {
        dataBuild();
        if (!this.currentRow.OK()) {
            throw new RuntimeException("not ok row");
        }
        currentRowBatch.AddRow(this.currentRow);
        Status status = new Status();
        this.currentRow = router.GetRequestRow(db, currentSql, status);
        if (status.getCode() != 0 || this.currentRow == null) {
            logger.error("getRequestRow failed: {}", status.getMsg());
            throw new SQLException("getRequestRow failed!, msg: " + status.getMsg());
        }
        status.delete();
    }

    @Override
    public void clearBatch() throws SQLException {
        currentRowBatch.Clear();
    }

    @Override
    public int[] executeBatch() throws SQLException {
        throw new SQLException("Should use executeQuery() to get batch result");
    }

    @Override
    public void close() throws SQLException {
        super.close();
        this.commonColumnIndices = null;
        this.currentRowBatch = null;
    }
}
