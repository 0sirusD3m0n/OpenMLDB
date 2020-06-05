package com._4paradigm.rtidb.client.ha;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com._4paradigm.rtidb.client.ha.TableHandler.ReadStrategy;

import rtidb.api.TabletServer;

public class PartitionHandler {
    private final static Logger logger = LoggerFactory.getLogger(PartitionHandler.class);
    private final static Random rand = new Random(System.currentTimeMillis());
    private TabletServerWapper leader = null;
    private List<TabletServerWapper> followers = new ArrayList<TabletServerWapper>();
    // the fast server for tablet read
    private TabletServerWapper fastTablet = null;

    public TabletServer getLeader() {
        return leader.getServer();
    }

    public void setLeader(TabletServerWapper leader) {
        this.leader = leader;
    }

    public List<TabletServerWapper> getFollowers() {
        return followers;
    }

    public void setFollowers(List<TabletServerWapper> followers) {
        this.followers = followers;
    }

    public TabletServer getFastTablet() {
        return fastTablet.getServer();
    }

    public void setFastTablet(TabletServerWapper fastTablet) {
        this.fastTablet = fastTablet;
    }

    public TabletServer getReadHandler(ReadStrategy strategy) {
        return getTabletServerWapper(strategy).getServer();
    }

    public TabletServerWapper getTabletServerWapper(ReadStrategy strategy) {
        // single node tablet
        if (followers.size() <= 0 || strategy == null) {
            return leader;
        }
        switch (strategy) {
            case kReadLeader:
                logger.debug("choose leader partition for reading");
                return leader;
            case kReadFollower:
                if (followers.size() > 0) {
                    logger.debug("rand choose follower partition for reading");
                    int index = rand.nextInt(followers.size());
                    return followers.get(index);
                } else {
                    logger.debug("choose leader partition for reading");
                    return leader;
                }
            case kReadLocal:
                if (fastTablet != null) {
                    logger.debug("choose fast partition for reading");
                    return fastTablet;
                } else if (followers.size() > 0) {
                    logger.debug("rand choose follower partition for reading");
                    int index = rand.nextInt(followers.size());
                    return followers.get(index);
                } else {
                    return leader;
                }
            case kReadRandom:
                logger.debug("rand choose partition for reading");
                if (followers.size() == 0) {
                    logger.debug("choose leader partition for reading");
                    return leader;
                } else {
                    int index = rand.nextInt(followers.size() + 1);
                    if (index == followers.size()) {
                        logger.debug("choose leader partition for reading");
                        return leader;
                    } else {
                        logger.debug("choose follower partition for reading");
                        return followers.get(index);
                    }
                }
            default:
                return leader;
        }
    }


}
