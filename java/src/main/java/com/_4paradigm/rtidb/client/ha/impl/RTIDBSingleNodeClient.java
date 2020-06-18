package com._4paradigm.rtidb.client.ha.impl;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import com._4paradigm.rtidb.client.ha.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com._4paradigm.rtidb.client.schema.ColumnDesc;
import com._4paradigm.rtidb.client.schema.SchemaCodec;
import com._4paradigm.rtidb.tablet.Tablet;
import com._4paradigm.rtidb.ns.NS;
import com._4paradigm.rtidb.ns.NS.TableInfo;

import io.brpc.client.EndPoint;
import io.brpc.client.RpcBaseClient;
import io.brpc.client.RpcClientOptions;
import io.brpc.client.RpcProxy;
import io.brpc.client.SingleEndpointRpcClient;
import rtidb.api.TabletServer;

public class RTIDBSingleNodeClient implements RTIDBClient {
    private final static Logger logger = LoggerFactory.getLogger(RTIDBSingleNodeClient.class);
    private EndPoint endpoint;
    private ConcurrentHashMap<String, TableHandler> name2tables = new ConcurrentHashMap<String, TableHandler>();
    private ConcurrentHashMap<Integer, TableHandler> id2tables = new ConcurrentHashMap<Integer, TableHandler>();
    private RpcBaseClient baseClient;
    private NodeManager nodeManager;
    private RTIDBClientConfig config;
    private TabletServerWapper tabletServerWapper;
    private SingleEndpointRpcClient singleNodeClient ;
    public RTIDBSingleNodeClient(RTIDBClientConfig config, EndPoint endpoint) {
        this.config = config;
        this.endpoint = endpoint;
    }

    public void init() throws Exception {
        RpcClientOptions options = new RpcClientOptions();
        options.setIoThreadNum(config.getIoThreadNum());
        options.setMaxConnectionNumPerHost(config.getMaxCntCnnPerHost());
        options.setReadTimeoutMillis(config.getReadTimeout());
        options.setWriteTimeoutMillis(config.getWriteTimeout());
        options.setMaxTryTimes(config.getMaxRetryCnt());
        baseClient = new RpcBaseClient(options);
        nodeManager = new NodeManager(baseClient);
        Set<EndPoint> nodes = new HashSet<EndPoint>();
        nodes.add(endpoint);
        nodeManager.update(nodes);
        singleNodeClient = new SingleEndpointRpcClient(baseClient);
        singleNodeClient.updateEndpoint(endpoint, nodeManager.getChannel(endpoint));
        TabletServer tabletServer = (TabletServer) RpcProxy.getProxy(singleNodeClient, TabletServer.class);
        tabletServerWapper = new TabletServerWapper(endpoint.getIp(), tabletServer);
        TableHandler th = new TableHandler();
        PartitionHandler ph = new PartitionHandler();
        ph.setLeader(tabletServerWapper);
        th.setPartitions(new PartitionHandler[] {ph});
        id2tables.putIfAbsent(0, th);
        logger.info("start single rtidb client with endpoint {} ok", endpoint);
    }


    public TableHandler getHandler(String name) {
        return name2tables.get(name);
    }

    public TableHandler getHandler(int id) {
        TableHandler th = id2tables.get(id);
        if (th == null) {
            Tablet.GetTableSchemaRequest request = Tablet.GetTableSchemaRequest.newBuilder().setTid(id).setPid(0)
                    .build();
            Tablet.GetTableSchemaResponse response = tabletServerWapper.getServer().getTableSchema(request);
            Tablet.GetTableStatusRequest statusRequest = Tablet.GetTableStatusRequest.newBuilder().build();
            Tablet.GetTableStatusResponse statusResponse = tabletServerWapper.getServer().getTableStatus(statusRequest);
            if (response.getCode() == 0 && statusResponse.getCode() == 0) {
                if (response.getSchema().isEmpty()) {
                    // some table maybe have no schema, eg kv table
                    th = new TableHandler();
                } else {
                    List<ColumnDesc> schema = SchemaCodec.decode(response.getSchema().asReadOnlyByteBuffer());
                    th = new TableHandler(schema);
                }
                for (Tablet.TableStatus status : statusResponse.getAllTableStatusList()) {
                    if (status.getTid() == id) {
                        TableInfo.Builder builder = TableInfo.newBuilder();
                        builder.setTid(status.getTid()).setTtl(status.getTtl());
                        if (status.hasCompressType() && status.getCompressType() == Tablet.CompressType.kSnappy) {
                            builder.setCompressType(NS.CompressType.kSnappy);
                        }
                        th.setTableInfo(builder.build());
                        break;
                    }
                }
                PartitionHandler ph = new PartitionHandler();
                ph.setLeader(tabletServerWapper);
                th.setPartitions(new PartitionHandler[] {ph});
                id2tables.putIfAbsent(id, th);
                return th;

            } else {
                return null;
            }
        }
        return th;
    }

    @Override
    public void close() {
        if (baseClient != null) {
            baseClient.stop();
        }
        if (nodeManager != null) {
            nodeManager.close();
        }
    }

    @Override
    public RTIDBClientConfig getConfig() {
        return config;
    }
    
}
