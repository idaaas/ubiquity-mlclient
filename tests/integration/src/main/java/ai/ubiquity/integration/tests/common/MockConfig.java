package ai.ubiquity.integration.tests.common;

import java.util.ArrayList;

import javax.naming.NameNotFoundException;

import ai.ubiquity.integration.tests.common.types.ApplicationIdType;
import ai.ubiquity.integration.tests.common.types.FeatureType;
import ai.ubiquity.integration.tests.common.types.ModelSchema;
import com.typesafe.config.*;
import java.util.List;
import java.util.concurrent.locks.Condition;

public class MockConfig {

  public static class ServerConf {
    public String ip = "";
    public boolean singleton = false;
    public List<Long> models = new ArrayList<>();
  }

  static ServerConf getServerConfFromId(String id) throws NameNotFoundException {
    ServerConf sConf = new ServerConf();

    List<? extends Config> nodes = ConfigFactory.load().getConfigList("cluster");

    for (Config node : nodes) {
      if (id.equals(node.getString("id"))) {
        sConf.ip = node.getString("ip");
        try {
          sConf.singleton = node.getBoolean("singleton");
        } catch (Exception e) {
          sConf.singleton = false;
        }

        try {
          sConf.models = node.getLongList("models");
        } catch (Exception ignored) {}

        return sConf;
      }
    }

    throw new NameNotFoundException("id [" + id + "] was not found in mock conf file");
  }

  static ServerConf getSingletonConf() throws NameNotFoundException {
    ServerConf sConf = new ServerConf();

    List<? extends Config> nodes = ConfigFactory.load().getConfigList("cluster");

    for (Config node : nodes) {
      try {
        sConf.singleton = node.getBoolean("singleton");
        if (!sConf.singleton) continue;

        sConf.ip = node.getString("ip");
        return sConf;
      } catch (Exception e) {
        System.out.println("singleton not found");
      }
    }
    return null;
  }

  public static ArrayList<String> getIpList() {
    ArrayList<String> ipList = new ArrayList<>();

    List<? extends Config> nodes = ConfigFactory.load().getConfigList("cluster");

    for (Config node : nodes) {
      ipList.add(node.getString("ip"));
    }

    return ipList;
  }

  static int getNodeNumber() {
    List<? extends Config> nodes = ConfigFactory.load().getConfigList("cluster");

    return nodes.size();
  }

  public static List<ModelSchema> getSchemas() {
    List<? extends Config> models;
    List<ModelSchema> schemas = new ArrayList<>();

    try {
      models = ConfigFactory.load().getConfigList("models");
    } catch (Exception e) {
      return schemas;
    }

    for (Config model: models) {
      long modelId = model.getLong("model_id");
      ApplicationIdType applicationId = ApplicationIdType.fromName(model.getString("application_id"));

      List<Integer> feedbacks = model.getIntList("feedback_delays");
      List<FeatureType> feat = new ArrayList<>();
      List<? extends Config> features = model.getConfigList("features");
      for (Config featConf: features) {
        feat.add(FeatureType.fromName(featConf.getString("type")));
      }

      schemas.add(new ModelSchema(modelId, applicationId, feedbacks, feat));
    }

    return schemas;
  }
}
