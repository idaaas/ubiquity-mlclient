package ai.ubiquity.integration.tests.common;

import ai.ubiquity.integration.tests.common.serialization.*;
import ai.ubiquity.integration.tests.common.serialization.TopologyUpdateResponse.NodeStatus;

import java.util.*;
import java.util.function.Function;

import javax.naming.NameNotFoundException;

import ai.ubiquity.integration.tests.common.types.InputConnectionMessageType;
import ai.ubiquity.integration.tests.common.types.InputMessageType;
import ai.ubiquity.integration.tests.common.types.MockRule;
import ai.ubiquity.integration.tests.common.types.ModelSchema;
import org.agrona.DirectBuffer;
import org.agrona.MutableDirectBuffer;
import org.agrona.concurrent.EpochClock;
import org.agrona.concurrent.SystemEpochClock;

import static java.lang.Thread.sleep;

public class MockProtocol {

  private HeaderDecoder headerReader = new HeaderDecoder();
  private final String id;
  private final static int PREDICTION_PORT = 2004;
  private short assignedClientId = 0x01;

  private final AeronConnectionManager aeronMngr;
  private final RuleSelector protocolBehaviour;
  protected EpochClock clock = SystemEpochClock.INSTANCE;

  private final Queue<Function<MutableDirectBuffer, AbstractMessage>> backPressureQueue;

  // TODO rentrer working buffer ici

  MockProtocol(String id, AeronConnectionManager aeronMngr) {
    this.id = id;
    this.aeronMngr = aeronMngr;
    this.protocolBehaviour = ProtocolParser.parseMessageHandlerConf(this.id);
    this.backPressureQueue = new LinkedList<>();
  }

  AbstractMessage handleMessage(MutableDirectBuffer workingBuffer, DirectBuffer buffer, int offset) {
    long startTime = clock.time();

    MockRule mock = applyMockBehaviour(workingBuffer, buffer, offset);
    System.out.printf("[Server %s] apply " + mock + "%n", this.id);
    try {

      AbstractMessage message = switch (mock.send()) {
        case TOPOLOGY_UPDATE -> buildTopologyResponse(workingBuffer);
        case CONNECTION_RESPONSE__REGISTRATION_RESPONSE -> buildRegistrationResponse(workingBuffer, buffer, offset);
        case CONNECTION_RESPONSE__DISCONNECTION_CONFIRMATION_RESPONSE ->
                buildDisconnectionConfirmationResponse(workingBuffer, buffer, offset);
        case CONNECTION_RESPONSE__SERVER_OPENED_RESPONSE -> buildServerOpenedResponse(workingBuffer, buffer, offset);
        case CONNECTION_RESPONSE__SERVER_READY_RESPONSE -> buildServerReadyResponse(workingBuffer, buffer, offset);
        case REGISTRATION_ERROR_RESPONSE__CORRELATION_CONFLICT ->
                buildCorrelationConflict(workingBuffer, buffer, offset);
        case REGISTRATION_ERROR_RESPONSE__NO_NEW_CONNECTION -> buildNoNewConnection(workingBuffer, buffer, offset);
        case MODEL_SCHEMA_UPDATE -> buildModelSchemaUpdate(workingBuffer);
        case NONE, START, SHUTDOWN__UNGRACEFUL, TOPOLOGY_UPDATE__REMOVE, ADD_MODEL, DISCONNECT, REMOVE_MODEL -> null;
      };

      if (mock.waitMs() != 0) sleep(clock.time() - startTime + mock.waitMs());
      return message;
    } catch (NameNotFoundException e) {
      System.out.printf("[Server %s] Mock conf not found : %s%n", this.id, e);
      return null;
    } catch (InterruptedException e) {
      System.out.printf("[Server %s] Interrupted exception while waiting to send message: %s%n", this.id, e);
      return null;
    }
  }

  private MockRule applyMockBehaviour(MutableDirectBuffer workingBuffer, DirectBuffer buffer, int offset) {
    this.headerReader.wrap(buffer, offset);
    InputMessageType type = InputMessageType.fromShort((short) headerReader.getType());
    InputConnectionMessageType subType = extractSubType(buffer, offset, type);

    System.out.println("received type: " + type.name + " and subtype: " + subType.name);

    return this.protocolBehaviour.getAppliedProtocol(type, subType, id);
  }

  private static InputConnectionMessageType extractSubType(DirectBuffer buffer, int offset, InputMessageType type) {
    InputConnectionMessageType subType = InputConnectionMessageType.NONE;
    if (type.equals(InputMessageType.CONNECTION_REQUEST)) {
      RegistrationRequestDecoder request = new RegistrationRequestDecoder();
      request.wrap(buffer, offset);
      subType = InputConnectionMessageType.fromShort(request.getOutput());
    }
    return subType;
  }

  AbstractMessage buildTopologyResponse(MutableDirectBuffer workingBuffer) throws NameNotFoundException {
    TopologyUpdateResponse message = new TopologyUpdateResponse();
    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setBroadcast();

    MockConfig.ServerConf serverConf = MockConfig.getServerConfFromId(id);
    if (serverConf.singleton) {
      message.setNodesStatus(NodeStatus.FULL_STATE);
      ArrayList<String> ips = MockConfig.getIpList();
      message.setNumNodes((short) ips.size());
      for (int i = 0; i < ips.size(); i++)
        message.setNodeIp(i, ips.get(i));
    } else {
      // serverConf = MockConfig.getSingletonConf();
      message.setNodesStatus(NodeStatus.ADDED);
      message.setNumNodes((short) 1);
      message.setNodeIp(0, serverConf.ip);
    }

    return message;
  }

  AbstractMessage buildCorrelationConflict(MutableDirectBuffer workingBuffer,
                                           DirectBuffer buffer, int offset) throws NameNotFoundException {
    RegistrationRequestDecoder request = new RegistrationRequestDecoder();
    request.wrap(buffer, offset);

    RegistrationErrorMessage message = new RegistrationErrorMessage();

    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setCorrelationId(request.getCorrelationIdHigh(), request.getCorrelationIdLow());
    message.setErrorCode(-2);

    return message;
  }

  AbstractMessage buildNoNewConnection(MutableDirectBuffer workingBuffer,
                                       DirectBuffer buffer, int offset) throws NameNotFoundException {
    RegistrationRequestDecoder request = new RegistrationRequestDecoder();
    request.wrap(buffer, offset);

    RegistrationErrorMessage message = new RegistrationErrorMessage();

    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setCorrelationId(request.getCorrelationIdHigh(), request.getCorrelationIdLow());
    message.setErrorCode(-1);

    return message;
  }

  AbstractMessage buildRegistrationResponse(MutableDirectBuffer workingBuffer,
                                            DirectBuffer buffer, int offset) throws NameNotFoundException {
    RegistrationRequestDecoder request = new RegistrationRequestDecoder();
    request.wrap(buffer, offset);

    RegistrationResponse message = new RegistrationResponse();

    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setCorrelationId(request.getCorrelationIdHigh(), request.getCorrelationIdLow());
    message.setControlPort((short) (PREDICTION_PORT + assignedClientId));
    message.setClientId(assignedClientId);

    if (!this.aeronMngr.isPredOpened()) {
      this.aeronMngr.openPred(PREDICTION_PORT + assignedClientId);
    }

    //TODO map client and aeron connections
    assignedClientId++;

    return message;
  }

  AbstractMessage buildDisconnectionConfirmationResponse(MutableDirectBuffer workingBuffer,
                                                         DirectBuffer buffer, int offset) throws NameNotFoundException {
    RegistrationRequestDecoder request = new RegistrationRequestDecoder();
    request.wrap(buffer, offset);

    DisconnectionConfirmationResponse message = new DisconnectionConfirmationResponse();
    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setClientId(request.getClientId());

    return message;
  }

  AbstractMessage buildServerOpenedResponse(MutableDirectBuffer workingBuffer,
                                            DirectBuffer buffer, int offset) throws NameNotFoundException {
    RegistrationRequestDecoder request = new RegistrationRequestDecoder();
    request.wrap(buffer, offset);

    ServerOpenedResponse message = new ServerOpenedResponse();
    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setClientId(request.getClientId());

    return message;
  }

  AbstractMessage buildServerReadyResponse(MutableDirectBuffer workingBuffer,
                                           DirectBuffer buffer, int offset) throws NameNotFoundException {
    RegistrationRequestDecoder request = new RegistrationRequestDecoder();
    request.wrap(buffer, offset);

    ServerReadyResponse message = new ServerReadyResponse();
    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setClientId(request.getClientId());

    return message;
  }

  AbstractMessage buildModelSchemaUpdate(MutableDirectBuffer workingBuffer) throws NameNotFoundException {
    return buildModelSchemaUpdate(workingBuffer, 0);
  }

  AbstractMessage buildModelSchemaUpdate(MutableDirectBuffer workingBuffer, int modelIndex) throws NameNotFoundException {
    MockConfig.ServerConf serverConf = MockConfig.getServerConfFromId(id);

    if (serverConf.models.isEmpty()) return null;

    // put additional message in queue to send right after
    if (modelIndex == 0 && serverConf.models.size() > 1) {
      for (int i = 1; i < serverConf.models.size(); i++) {
        int finalI = i;
        System.out.printf("[Server %s] pushing message in queue%n", this.id);
        backPressureQueue.add((wb) -> {
          try {
            return buildModelSchemaUpdate(wb, finalI);
          } catch (NameNotFoundException e) {
            System.out.printf("[Server %s] error in message dequeue %s %n", this.id, e);
            throw new RuntimeException(e);
          }
        });
      }
    }

    long modelId = serverConf.models.get(modelIndex);
    return getModelSchemaUpdateReponse(workingBuffer, modelId);
  }

  private ModelSchemaUpdateReponse getModelSchemaUpdateReponse(MutableDirectBuffer workingBuffer, long modelId) throws NameNotFoundException {
    MockConfig.ServerConf serverConf = MockConfig.getServerConfFromId(id);
    System.out.printf("[Server %s] Sending model Id %s %n", this.id, modelId);

    Optional<ModelSchema> modelMonad = MockConfig.getSchemas()
            .stream()
            .filter(modelSchema -> modelSchema.modelId() == modelId)
            .findFirst();
    if (modelMonad.isEmpty()) return null;

    ModelSchema model = modelMonad.get();

    ModelSchemaUpdateReponse message = new ModelSchemaUpdateReponse();
    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setNodeIp(serverConf.ip);
    message.setModelId(modelId);
    message.setApplicationId(model.appId());
    message.setFeedbackDelays(model.feedbackDelays(), model.featureTypes());
    return message;
  }

  public AbstractMessage handleBackPressureQueue(MutableDirectBuffer workingBuffer) {
    if (this.backPressureQueue.isEmpty()) return null;

    System.out.printf("[Server %s] dequeue message%n", this.id);

    return Objects.requireNonNull(this.backPressureQueue.poll()).apply(workingBuffer);
  }

  public AbstractMessage handleTimeouts(MutableDirectBuffer workingBuffer, long elapsedTime) throws NameNotFoundException {
    MockRule rule = protocolBehaviour.getAppliedProtocolWithTimeouts(InputMessageType.TIME_MS, elapsedTime, id);
    if (rule == null) return null;

    System.out.printf("[Server %s] apply " + rule + "%n", this.id);

    return switch (rule.send()) {
      case MODEL_SCHEMA_UPDATE -> buildModelSchemaUpdate(workingBuffer);
      case TOPOLOGY_UPDATE, NONE, CONNECTION_RESPONSE__REGISTRATION_RESPONSE,
           CONNECTION_RESPONSE__DISCONNECTION_CONFIRMATION_RESPONSE, CONNECTION_RESPONSE__SERVER_OPENED_RESPONSE,
           CONNECTION_RESPONSE__SERVER_READY_RESPONSE, REGISTRATION_ERROR_RESPONSE__CORRELATION_CONFLICT,
           REGISTRATION_ERROR_RESPONSE__NO_NEW_CONNECTION -> null;
      case TOPOLOGY_UPDATE__REMOVE -> handleRemoveNode(workingBuffer);
      case SHUTDOWN__UNGRACEFUL -> handleUngratefulShutdown();
      case START -> handleStart();
      case DISCONNECT -> handleDisconnect();
      case ADD_MODEL -> handeAddModel(workingBuffer, rule.what());
      case REMOVE_MODEL -> handleRemoveModel(workingBuffer, rule.what());
    };
  }

  private AbstractMessage handleRemoveModel(MutableDirectBuffer workingBuffer, String ruleModelId) throws NameNotFoundException {
    MockConfig.ServerConf serverConf = MockConfig.getServerConfFromId(id);
    System.out.printf("[Server %s] Removing model Id %s %n", this.id, ruleModelId);

    ModelRemoveResponse message = new ModelRemoveResponse();
    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setNodeIp(serverConf.ip);
    message.setModelId(Long.parseLong(ruleModelId));
    return message;
  }

  private AbstractMessage handeAddModel(MutableDirectBuffer workingBuffer, String ruleModelId) {
    try {
      return getModelSchemaUpdateReponse(workingBuffer, Long.parseLong(ruleModelId));
    } catch (NameNotFoundException e) {
      return null;
    }
  }

  private AbstractMessage handleRemoveNode(MutableDirectBuffer workingBuffer) throws NameNotFoundException {
    TopologyUpdateResponse message = new TopologyUpdateResponse();

    this.aeronMngr.areProtocolStreamsOpened();
    message.wrap(workingBuffer, 0);
    message.setHeader();
    message.setBroadcast();

    MockConfig.ServerConf serverConf = MockConfig.getServerConfFromId(id);
    message.setNodesStatus(NodeStatus.REMOVED);
    message.setNumNodes((short) 1);
    message.setNodeIp(0, serverConf.ip);

    return message;
  }

  private AbstractMessage handleDisconnect() {
    this.aeronMngr.closePred();
    //this.aeronMngr.closeMulticast();
    return null;
  }

  private AbstractMessage handleStart() {
    this.aeronMngr.areProtocolStreamsOpened();
    //this.aeronMngr.connectMulticast();
    this.aeronMngr.areProtocolStreamsOpened();
    return null;
  }

  private AbstractMessage handleUngratefulShutdown() {
    this.aeronMngr.restartPred();
    return null;
  }
}
