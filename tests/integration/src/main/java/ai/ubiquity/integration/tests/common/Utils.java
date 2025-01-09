package ai.ubiquity.integration.tests.common;

import ai.ubiquity.integration.tests.common.MockServer;

import io.aeron.driver.MediaDriver;
import io.aeron.Aeron;
import io.aeron.Image;
import io.aeron.Subscription;
import io.aeron.driver.ThreadingMode;
import org.agrona.CloseHelper;

import com.typesafe.config.ConfigFactory;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;

public class Utils {
    public static MediaDriver launchEmbeddedMediaDriver(String scenarioId) {
        String aeronDir = "/dev/shm/aeron-folder-" + scenarioId + "-" + System.currentTimeMillis();
        final MediaDriver.Context context = new MediaDriver.Context()
                .dirDeleteOnStart(true)
                .dirDeleteOnShutdown(true)
                .threadingMode(ThreadingMode.SHARED)
                .publicationTermBufferLength(65536)
                .aeronDirectoryName(aeronDir);
        return MediaDriver.launchEmbedded(context);
    }

    public static Aeron initAeron(MediaDriver driver) {
        final Aeron.Context ctx = new Aeron.Context()
                .availableImageHandler(Utils::availableImageHandler)
                .unavailableImageHandler(Utils::unavailableImageHandler)
                .aeronDirectoryName(driver.aeronDirectoryName());
        Aeron aeron;
        try {
            aeron = Aeron.connect(ctx);
        } catch (Exception e) {
            CloseHelper.close(driver);
            throw new IllegalStateException("Scenario failed! could not instantiate Aeron instance");
        }
        return aeron;
    }

    static void availableImageHandler(Image image) {
        final Subscription subscription = image.subscription();
        System.out.printf("Available image. channel=%s stream_id=%s client=%s%n",
                subscription.channel(), subscription.streamId(), image.sourceIdentity());
    }

    static void unavailableImageHandler(Image image) {
        final Subscription subscription = image.subscription();
        System.out.printf("Unavailable image. channel=%s stream_id=%s client=%s%n",
                subscription.channel(), subscription.streamId(), image.sourceIdentity());
    }
}
