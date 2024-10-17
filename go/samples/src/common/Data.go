package common

import (
	"encoding/csv"
	"log"
	"os"
	"strconv"
)

type BidRequest struct {
	AppBundle            string
	ImpBannerPos         int8
	LocalTime            int8
	DeviceOs             string
	DeviceGeoCity        string
	DeviceModel          string
	Bcat                 string
	DeviceGeoZip         string
	UserYob              string
	AppPublisherName     string
	DeviceConnectionType int8
	DeviceGeoRegion      string
	AppCat               string
	DeviceMake           string
	DeviceHxW            string
	UserGender           int8
	DeviceLanguage       string
	ImpDisplayManager    string
	ImpBannerBattr       string
	DeviceGeoCountry     string
	AppContentLanguage   string
	AppContentContext    int8
	ImpBannerTopFrame    bool
	DeviceType           int8
	ImpInstl             bool
	AppPageCat           string
}

type Feedback struct {
	Level1 bool
	Level2 bool
	Level3 bool
}

func LoadBidRequests(fileName string) []BidRequest {
	file, err := os.Open(fileName)
	if err != nil {
		log.Fatal(err)
	}
	defer file.Close()

	reader := csv.NewReader(file)
	records, err := reader.ReadAll()
	if err != nil {
		log.Fatal(err)
	}

	bidRequests := make([]BidRequest, len(records))
	for index, record := range records {
		bidRequests[index].AppBundle = record[0]
		val, _ := strconv.Atoi(record[1])
		bidRequests[index].ImpBannerPos = int8(val)
		val, _ = strconv.Atoi(record[2])
		bidRequests[index].LocalTime = int8(val)
		bidRequests[index].DeviceOs = record[3]
		bidRequests[index].DeviceGeoCity = record[4]
		bidRequests[index].DeviceModel = record[5]
		bidRequests[index].Bcat = record[6]
		bidRequests[index].DeviceGeoZip = record[7]
		bidRequests[index].UserYob = record[8]
		bidRequests[index].AppPublisherName = record[9]
		val, _ = strconv.Atoi(record[10])
		bidRequests[index].DeviceConnectionType = int8(val)
		bidRequests[index].DeviceGeoRegion = record[11]
		bidRequests[index].AppCat = record[12]
		bidRequests[index].DeviceMake = record[13]
		bidRequests[index].DeviceHxW = record[14]
		val, _ = strconv.Atoi(record[15])
		bidRequests[index].UserGender = int8(val)
		bidRequests[index].DeviceLanguage = record[16]
		bidRequests[index].ImpDisplayManager = record[17]
		bidRequests[index].ImpBannerBattr = record[18]
		bidRequests[index].DeviceGeoCountry = record[19]
		bidRequests[index].AppContentLanguage = record[20]
		val, _ = strconv.Atoi(record[21])
		bidRequests[index].AppContentContext = int8(val)
		bidRequests[index].ImpBannerTopFrame = "true" == record[22]
		val, _ = strconv.Atoi(record[23])
		bidRequests[index].DeviceType = int8(val)
		bidRequests[index].ImpInstl = "true" == record[24]
		bidRequests[index].AppPageCat = record[25]
	}
	return bidRequests
}

func LoadFeedbacks(fileName string) []Feedback {
	file, err := os.Open(fileName)
	if err != nil {
		log.Fatal(err)
	}
	defer file.Close()

	reader := csv.NewReader(file)
	records, err := reader.ReadAll()
	if err != nil {
		log.Fatal(err)
	}

	feedbacks := make([]Feedback, len(records))
	for index, record := range records {
		feedbacks[index].Level1 = "true" == record[0]
		feedbacks[index].Level2 = "true" == record[1]
		feedbacks[index].Level3 = "true" == record[2]
	}
	return feedbacks
}

func (self BidRequest) toString() string {
	return "BidRequest{" +
		"appBundle='" + self.AppBundle + "'" +
		", impBannerPos=" + strconv.Itoa(int(self.ImpBannerPos)) +
		", localTime=" + strconv.Itoa(int(self.LocalTime)) +
		", deviceOs='" + self.DeviceOs + "'" +
		", deviceGeoCity='" + self.DeviceGeoCity + "'" +
		", deviceModel='" + self.DeviceModel + "'" +
		", bcat='" + self.Bcat + "'" +
		", deviceGeoZip='" + self.DeviceGeoZip + "'" +
		", userYob='" + self.UserYob + "'" +
		", appPublisherName='" + self.AppPublisherName + "'" +
		", deviceConnectionType=" + strconv.Itoa(int(self.DeviceConnectionType)) +
		", deviceGeoRegion='" + self.DeviceGeoRegion + "'" +
		", appCat='" + self.AppCat + "'" +
		", deviceMake='" + self.DeviceMake + "'" +
		", deviceHxW='" + self.DeviceHxW + "'" +
		", userGender=" + strconv.Itoa(int(self.UserGender)) +
		", deviceLanguage='" + self.DeviceLanguage + "'" +
		", impDisplayManager='" + self.ImpDisplayManager + "'" +
		", impBannerBattr='" + self.ImpBannerBattr + "'" +
		", deviceGeoCountry='" + self.DeviceGeoCountry + "'" +
		", appContentLanguage='" + self.AppContentLanguage + "'" +
		", appContentContext=" + strconv.Itoa(int(self.AppContentContext)) +
		", impBannerTopFrame=" + strconv.FormatBool(self.ImpBannerTopFrame) +
		", deviceType=" + strconv.Itoa(int(self.DeviceType)) +
		", impInstl=" + strconv.FormatBool(self.ImpInstl) +
		", appPageCat='" + self.AppPageCat + "'" +
		"}"
}
func (self Feedback) toString() string {
	return "Feedback{" +
		"level1='" + strconv.FormatBool(self.Level1) + "'" +
		", level2='" + strconv.FormatBool(self.Level2) + "'" +
		", level3='" + strconv.FormatBool(self.Level3) + "'" +
		"}"
}
