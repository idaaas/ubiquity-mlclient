package ai.ubiquity.samples.utils;

import com.opencsv.bean.CsvToBeanBuilder;

import java.io.*;
import java.util.Iterator;
import java.util.function.Supplier;
import java.util.zip.GZIPInputStream;

/**
 * an iterator that cycles over an iterator infinitely.
 * it restarts the iterator when it ends.
 *
 * @param <E>
 */
public class CycleIterator<E> implements Iterator<E> {

    private final Supplier<Iterator<E>> iteratorFactory;

    private Iterator<E> iterator;

    public CycleIterator(Supplier<Iterator<E>> iteratorFactory) {
        this.iteratorFactory = iteratorFactory;
        try {
            this.iterator = iteratorFactory.get();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public boolean hasNext() {
        return true;
    }

    @Override
    public E next() {
        if (iterator.hasNext())
            return iterator.next();
        this.iterator = iteratorFactory.get();
        return iterator.next();
    }

    public static Iterator<BidRequest> bidRequestIterator(String inputPath) {
        return new CycleIterator<>(() -> {
            try {
                return new CsvToBeanBuilder<BidRequest>(new InputStreamReader(FileUtil.createInputStream(inputPath)))
                  .withType(BidRequest.class)
                  .build()
                  .iterator();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        });
    }

    public static Iterator<Feedback> feedbackIterator(String inputPath) {
        return new CycleIterator<>(() -> {
            try {
                return new CsvToBeanBuilder<Feedback>(new BufferedReader(new InputStreamReader(FileUtil.createInputStream(inputPath))))
                  .withType(Feedback.class)
                  .build()
                  .iterator();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        });
    }

    public static class FileUtil {
        public static InputStream createInputStream(String inputPath) throws IOException {
            if (new File(inputPath).exists()) {
                File file = new File(inputPath);
                if (isGZipped(file)) {
                    return new GZIPInputStream(new FileInputStream(file));
                } else {
                    return new FileInputStream(file);
                }
            } else {
                throw new FileNotFoundException(String.format("no file at %s was found", inputPath));
            }
        }

        public static boolean isGZipped(File f) {
            int magic = 0;
            try {
                RandomAccessFile raf = new RandomAccessFile(f, "r");
                magic = raf.read() & 0xff | ((raf.read() << 8) & 0xff00);
                raf.close();
            } catch (Throwable e) {
                e.printStackTrace(System.err);
            }
            return magic == GZIPInputStream.GZIP_MAGIC;
        }
    }
}
