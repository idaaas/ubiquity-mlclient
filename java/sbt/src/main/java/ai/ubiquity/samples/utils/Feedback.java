package ai.ubiquity.samples.utils;

import com.opencsv.bean.CsvBindByPosition;

public class Feedback {
    @CsvBindByPosition(position = 0)
    private boolean level1;
    @CsvBindByPosition(position = 1)
    private boolean level2;
    @CsvBindByPosition(position = 2)
    private boolean level3;

    public boolean getLevel1() {
        return level1;
    }

    public boolean getLevel2() {
        return level2;
    }

    public boolean getLevel3() {
        return level3;
    }

    @Override
    public String toString() {
        return "Feedback{" +
          "level1='" + level1 + '\'' +
          ", level2='" + level2 + '\'' +
          ", level3='" + level3 + '\'' +
          '}';
    }
}
