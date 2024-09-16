package ai.ubiquity.samples.utils;

import com.opencsv.bean.CsvBindByPosition;

public class BidRequest {
    @CsvBindByPosition(position = 0)
    private String appBundle;
    @CsvBindByPosition(position = 1)
    private byte impBannerPos;
    @CsvBindByPosition(position = 2)
    private byte localTime;
    @CsvBindByPosition(position = 3)
    private String deviceOs;
    @CsvBindByPosition(position = 4)
    private String deviceGeoCity;
    @CsvBindByPosition(position = 5)
    private String deviceModel;
    @CsvBindByPosition(position = 6)
    private String bcat;
    @CsvBindByPosition(position = 7)
    private String deviceGeoZip;
    @CsvBindByPosition(position = 8)
    private String userYob;
    @CsvBindByPosition(position = 9)
    private String appPublisherName;
    @CsvBindByPosition(position = 10)
    private byte deviceConnectionType;
    @CsvBindByPosition(position = 11)
    private String deviceGeoRegion;
    @CsvBindByPosition(position = 12)
    private String appCat;
    @CsvBindByPosition(position = 13)
    private String deviceMake;
    @CsvBindByPosition(position = 14)
    private String deviceHxW;
    @CsvBindByPosition(position = 15)
    private byte userGender;
    @CsvBindByPosition(position = 16)
    private String deviceLanguage;
    @CsvBindByPosition(position = 17)
    private String impDisplayManager;
    @CsvBindByPosition(position = 18)
    private String impBannerBattr;
    @CsvBindByPosition(position = 19)
    private String deviceGeoCountry;
    @CsvBindByPosition(position = 20)
    private String appContentLanguage;
    @CsvBindByPosition(position = 21)
    private byte appContentContext;
    @CsvBindByPosition(position = 22)
    private boolean impBannerTopFrame;
    @CsvBindByPosition(position = 23)
    private byte deviceType;
    @CsvBindByPosition(position = 24)
    private boolean impInstl;
    @CsvBindByPosition(position = 25)
    private String appPageCat;

    public String getAppBundle() {
        return appBundle;
    }

    public byte getImpBannerPos() {
        return impBannerPos;
    }

    public byte getLocalTime() {
        return localTime;
    }

    public String getDeviceOs() {
        return deviceOs;
    }

    public String getDeviceGeoCity() {
        return deviceGeoCity;
    }

    public String getDeviceModel() {
        return deviceModel;
    }

    public String getBcat() {
        return bcat;
    }

    public String getDeviceGeoZip() {
        return deviceGeoZip;
    }

    public String getUserYob() {
        return userYob;
    }

    public String getAppPublisherName() {
        return appPublisherName;
    }

    public byte getDeviceConnectionType() {
        return deviceConnectionType;
    }

    public String getDeviceGeoRegion() {
        return deviceGeoRegion;
    }

    public String getAppCat() {
        return appCat;
    }

    public String getDeviceMake() {
        return deviceMake;
    }

    public String getDeviceHxW() {
        return deviceHxW;
    }

    public byte getUserGender() {
        return userGender;
    }

    public String getDeviceLanguage() {
        return deviceLanguage;
    }

    public String getImpDisplayManager() {
        return impDisplayManager;
    }

    public String getImpBannerBattr() {
        return impBannerBattr;
    }

    public String getDeviceGeoCountry() {
        return deviceGeoCountry;
    }

    public String getAppContentLanguage() {
        return appContentLanguage;
    }

    public byte getAppContentContext() {
        return appContentContext;
    }

    public boolean getImpBannerTopFrame() {
        return impBannerTopFrame;
    }

    public byte getDeviceType() {
        return deviceType;
    }

    public boolean getImpInstl() {
        return impInstl;
    }

    public String getAppPageCat() {
        return appPageCat;
    }

    @Override
    public String toString() {
        return "BidRequest{" +
          "appBundle='" + appBundle + '\'' +
          ", impBannerPos=" + impBannerPos +
          ", localTime=" + localTime +
          ", deviceOs='" + deviceOs + '\'' +
          ", deviceGeoCity='" + deviceGeoCity + '\'' +
          ", deviceModel='" + deviceModel + '\'' +
          ", bcat='" + bcat + '\'' +
          ", deviceGeoZip='" + deviceGeoZip + '\'' +
          ", userYob='" + userYob + '\'' +
          ", appPublisherName='" + appPublisherName + '\'' +
          ", deviceConnectionType=" + deviceConnectionType +
          ", deviceGeoRegion='" + deviceGeoRegion + '\'' +
          ", appCat='" + appCat + '\'' +
          ", deviceMake='" + deviceMake + '\'' +
          ", deviceHxW='" + deviceHxW + '\'' +
          ", userGender=" + userGender +
          ", deviceLanguage='" + deviceLanguage + '\'' +
          ", impDisplayManager='" + impDisplayManager + '\'' +
          ", impBannerBattr='" + impBannerBattr + '\'' +
          ", deviceGeoCountry='" + deviceGeoCountry + '\'' +
          ", appContentLanguage='" + appContentLanguage + '\'' +
          ", appContentContext=" + appContentContext +
          ", impBannerTopFrame=" + impBannerTopFrame +
          ", deviceType=" + deviceType +
          ", impInstl=" + impInstl +
          ", appPageCat='" + appPageCat + '\'' +
          '}';
    }
}
