package assertion;

import java.io.BufferedReader;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

public class AssertionRule {
  public final AssertionRule then;
  public AssertionRule and;
  private int cursor, originalCursor = 0;
  private final AssertContext ctx;

  public AssertionRule(AssertContext ctx, int cursor) {
    this.ctx = ctx;
    this.cursor = cursor;
    this.originalCursor = cursor;
    then = this;
    and = this;
  }

  public AssertionRule(AssertContext ctx) {
    this.ctx = ctx;
    then = this;
    and = this;
  }

  public AssertionRule shouldLogAtLeast(String partialSentence, int leastTime) {
    int nbTimes = findAllPartialSentenceOccurrences(partialSentence, leastTime);
    assertTrue(nbTimes >= leastTime, "failed to match '"+partialSentence+"' enough");

    return next();
  }


  public AssertionRule shouldLog(String partialSentence) {
    return shouldLog(partialSentence, 1);
  }

  public AssertionRule shouldLog(String partialSentence, int expectedTimes) {
    int nbTimes = findPartialSentence(partialSentence, expectedTimes);
    assertEquals(expectedTimes, nbTimes, "failed to match '"+partialSentence+"'");

    return next();
  }

  public AssertionRule shouldNotLog(String partialSentence) {
    int nbTimes = findPartialSentence(partialSentence, 1);
    assertEquals(0, nbTimes, "should not have matched '"+partialSentence+"'");

    return next();
  }

  private AssertionRule next() {
    AssertionRule res = new AssertionRule(ctx, cursor);
    res.and = new AssertionRule(ctx, originalCursor);
    return res;
  }

  private String escape(String str) {
    return str.replace("[", "\\[").replace("(", "\\(").replace(")", "\\)");
  }

  private int findPartialSentence(String partialSentence, int times) {
    int nbFound = 0;
    int currentCursor = -1;
    String line;

    String regex;
    if (Objects.equals(ctx.identifier(), "")) {
      regex = String.format(".*%s.*", escape(partialSentence));
    } else {
      regex = String.format(".*(%s|%s).*(%s|%s).*", escape(ctx.identifier()), escape(partialSentence), escape(partialSentence), escape(ctx.identifier()));
    }

    Pattern pattern = Pattern.compile(regex);

    try (BufferedReader reader = Files.newBufferedReader(Paths.get(ctx.filePath()))) {
      while ((line = reader.readLine()) != null) {
        if (++currentCursor < cursor) continue;

        Matcher matcher = pattern.matcher(line);
        if (!matcher.find()) continue;

        nbFound++;
        if (nbFound == times) {
          cursor = currentCursor;
          cursor++;
          break;
        }
      }

      return nbFound;
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  private int findAllPartialSentenceOccurrences(String partialSentence, int times) {
    int nbFound = 0;
    int currentCursor = -1;
    String line;

    String regex = String.format(".*(%s|%s).*(%s|%s).*", escape(ctx.identifier()), escape(partialSentence), escape(partialSentence), escape(ctx.identifier()));
    Pattern pattern = Pattern.compile(regex);

    try (BufferedReader reader = Files.newBufferedReader(Paths.get(ctx.filePath()))) {
      while ((line = reader.readLine()) != null) {
        if (++currentCursor < cursor) continue;

        Matcher matcher = pattern.matcher(line);
        if (!matcher.find()) continue;

        nbFound++;
        if (nbFound == times) {
          cursor = currentCursor;
          cursor++;
        }
      }

      return nbFound;
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }


}
