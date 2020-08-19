#! /bin/sh
#
ulimit -c unlimited
# start_onebox.sh

# first start zookeeper
IP=127.0.0.1

ZK_CLUSTER=$IP:6181
NS1=$IP:9622
NS2=$IP:9623
NS3=$IP:9624
TABLET1=$IP:9520
TABLET2=$IP:9521
TABLET3=$IP:9522
BLOB1=$IP:9720

../build/bin/rtidb --db_root_path=/rambuild/tablet0-binlogs \
                   --hdd_root_path=/rambuild/tablet0-hdd-binlogs \
                   --ssd_root_path=/rambuild/tablet0-ssd-binlogs \
                   --recycle_bin_root_path=/rambuild/recycle_bin0 \
                   --recycle_ssd_bin_root_path=/rambuild/recycle_ssd_bin0 \
                   --recycle_hdd_bin_root_path=/rambuild/recycle_hdd_bin0 \
                   --endpoint=${TABLET1} --role=tablet \
                   --binlog_notify_on_put=true\
                   --zk_cluster=${ZK_CLUSTER}\
                   --zk_keep_alive_check_interval=100000000\
                   --zk_root_path=/onebox > tablet0.log 2>&1 &

# start tablet1
../build/bin/rtidb --db_root_path=/rambuild/tablet1-binlogs \
                   --hdd_root_path=/rambuild/tablet1-hdd-binlogs \
                   --ssd_root_path=/rambuild/tablet1-ssd-binlogs \
                   --recycle_bin_root_path=/rambuild/recycle_bin1 \
                   --recycle_ssd_bin_root_path=/rambuild/recycle_ssd-bin1 \
                   --recycle_hdd_bin_root_path=/rambuild/recycle_hdd-bin1 \
                   --endpoint=${TABLET2} --role=tablet \
                   --zk_cluster=${ZK_CLUSTER}\
                   --binlog_notify_on_put=true\
                   --zk_keep_alive_check_interval=100000000\
                   --zk_root_path=/onebox > tablet1.log 2>&1 &

# start tablet2
../build/bin/rtidb --db_root_path=/rambuild/tablet2-binlogs \
                   --hdd_root_path=/rambuild/tablet2-hdd-binlogs \
                   --ssd_root_path=/rambuild/tablet2-ssd-binlogs \
                   --recycle_bin_root_path=/rambuild/recycle_bin2 \
                   --recycle_ssd_bin_root_path=/rambuild/recycle_ssd_bin2 \
                   --recycle_hdd_bin_root_path=/rambuild/recycle_hdd_bin2 \
                   --endpoint=${TABLET3} --role=tablet \
                   --binlog_notify_on_put=true\
                   --zk_cluster=${ZK_CLUSTER}\
                   --zk_keep_alive_check_interval=100000000\
                   --zk_root_path=/onebox > tablet2.log 2>&1 &

# start ns1 
../build/bin/rtidb --endpoint=${NS1} --role=nameserver \
                   --zk_cluster=${ZK_CLUSTER}\
                   --tablet_offline_check_interval=1\
                   --tablet_heartbeat_timeout=1\
                   --request_timeout_ms=200000\
                   --zk_root_path=/onebox > ns1.log 2>&1 &
sleep 2

# start ns2 
../build/bin/rtidb --endpoint=${NS2} --role=nameserver \
                   --zk_cluster=${ZK_CLUSTER}\
                   --tablet_offline_check_interval=1\
                   --tablet_heartbeat_timeout=1\
                   --request_timeout_ms=200000\
                   --zk_root_path=/onebox > ns2.log 2>&1 &
sleep 2

# start ns3 
../build/bin/rtidb --endpoint=${NS3} --role=nameserver \
                   --tablet_offline_check_interval=1\
                   --tablet_heartbeat_timeout=1\
                   --request_timeout_ms=200000\
                   --zk_cluster=${ZK_CLUSTER}\
                   --zk_root_path=/onebox > ns3.log 2>&1 &

sleep 5
# start blob1
../build/bin/rtidb --hdd_root_path=/rambuild/blob1-hdd-binlogs \
                   --ssd_root_path=/rambuild/blob1-ssd-binlogs \
                   --recycle_bin_root_path=/rambuild/recycle_bin3 \
                   --recycle_ssd_bin_root_path=/rambuild/recycle_ssd_bin3 \
                   --recycle_hdd_bin_root_path=/rambuild/recycle_hdd_bin3 \
                   --endpoint=${BLOB1} --role=blob \
                   --binlog_notify_on_put=true\
                   --zk_cluster=${ZK_CLUSTER}\
                   --zk_keep_alive_check_interval=100000000\
                   --zk_root_path=/onebox > blob1.log 2>&1 &

sleep 5

echo "start all ok"

