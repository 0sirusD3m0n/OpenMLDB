package com._4paradigm.sql.sdk;

import com._4paradigm.sql.DataType;
import com._4paradigm.sql.Schema;

import java.sql.SQLException;
import java.sql.Types;
import java.util.ArrayList;
import java.util.List;

public class Common {
    public static int type2SqlType(DataType dataType) throws SQLException {
        if (dataType == DataType.kTypeBool) {
            return Types.BOOLEAN;
        } else if (dataType == DataType.kTypeInt16) {
            return Types.SMALLINT;
        } else if (dataType == DataType.kTypeInt32) {
            return Types.INTEGER;
        } else if (dataType == DataType.kTypeInt64) {
            return Types.BIGINT;
        } else if (dataType == DataType.kTypeFloat) {
            return Types.FLOAT;
        } else if (dataType == DataType.kTypeDouble) {
            return Types.DOUBLE;
        } else if (dataType == DataType.kTypeString) {
            return Types.VARCHAR;
        } else if (dataType == DataType.kTypeDate) {
            return Types.DATE;
        } else if (dataType == DataType.kTypeTimestamp) {
            return Types.TIMESTAMP;
        } else {
            throw new SQLException("Unexpected value: " + dataType.toString());
        }
    }

    public static com._4paradigm.sql.sdk.Schema convertSchema(Schema schema) throws SQLException {
        if (schema == null || schema.GetColumnCnt() == 0) {
            throw new SQLException("schema is null or empty");
        }
        List<Column> columnList = new ArrayList<>();
        for (int i = 0; i < schema.GetColumnCnt(); i++) {
            Column column = new Column();
            column.setColumnName(schema.GetColumnName(i));
            column.setSqlType(type2SqlType(schema.GetColumnType(i)));
            column.setNotNull(schema.IsColumnNotNull(i));
            column.setConstant(schema.IsConstant(i));
            columnList.add(column);
        }
        return new com._4paradigm.sql.sdk.Schema(columnList);
    }
}
