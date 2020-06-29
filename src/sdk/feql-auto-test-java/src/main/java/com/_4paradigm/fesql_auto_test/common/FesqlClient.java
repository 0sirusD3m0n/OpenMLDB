package com._4paradigm.fesql_auto_test.common;

import com._4paradigm.sql.sdk.SdkOption;
import com._4paradigm.sql.sdk.SqlException;
import com._4paradigm.sql.sdk.SqlExecutor;
import com._4paradigm.sql.sdk.impl.SqlClusterExecutor;
import lombok.Data;
import lombok.extern.slf4j.Slf4j;

/**
 * @author zhaowei
 * @date 2020/6/11 11:28 AM
 */
@Data
@Slf4j
public class FesqlClient {

    private SqlExecutor executor;

    public FesqlClient(String zkCluster,String zkPath){
        SdkOption option = new SdkOption();
        option.setZkCluster(zkCluster);
        option.setZkPath(zkPath);
        option.setSessionTimeout(1000000);
        try {
            executor = new SqlClusterExecutor(option);
        } catch (SqlException e) {
            e.printStackTrace();
        }
    }
}
