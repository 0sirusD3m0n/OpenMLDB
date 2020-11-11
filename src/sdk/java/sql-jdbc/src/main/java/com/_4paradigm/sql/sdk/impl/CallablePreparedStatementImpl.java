package com._4paradigm.sql.sdk.impl;

import com._4paradigm.sql.QueryFuture;
import com._4paradigm.sql.SQLRouter;
import com._4paradigm.sql.Status;
import com._4paradigm.sql.jdbc.CallablePreparedStatement;
import com._4paradigm.sql.jdbc.SQLResultSet;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.concurrent.TimeUnit;

public class CallablePreparedStatementImpl extends CallablePreparedStatement {
    private static final Logger logger = LoggerFactory.getLogger(CallablePreparedStatementImpl.class);

    public CallablePreparedStatementImpl(String db, String spName, SQLRouter router) throws SQLException{
        super(db, spName, router);
    }

    @Override
    public java.sql.ResultSet executeQuery() throws SQLException {
        checkClosed();
        dataBuild();
        Status status = new Status();
        com._4paradigm.sql.ResultSet resultSet = router.CallProcedure(db, spName, currentRow, status);
        if (status.getCode() != 0 || resultSet == null) {
            logger.error("call procedure failed: {}", status.getMsg());
            throw new SQLException("call procedure fail, msg: " + status.getMsg());
        }
        ResultSet rs = new SQLResultSet(resultSet);
        if (closeOnComplete) {
            closed = true;
        }
        return rs;
    }

    public com._4paradigm.sql.sdk.QueryFuture executeQeuryAsyn(long timeOut, TimeUnit unit) throws SQLException {
        checkClosed();
        dataBuild();
        Status status = new Status();
        QueryFuture queryFuture = router.CallProcedure(db, spName, unit.toMillis(timeOut), currentRow, status);
        if (status.getCode() != 0 || queryFuture == null) {
            logger.error("call procedure failed: {}", status.getMsg());
            throw new SQLException("call procedure fail, msg: " + status.getMsg());
        }
        return new com._4paradigm.sql.sdk.QueryFuture(queryFuture);
    }

}
