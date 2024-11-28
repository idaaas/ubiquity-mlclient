import csv
import os
from dataclasses import dataclass
from typing import List

@dataclass
class BidRequest:
    app_bundle: str
    imp_banner_pos: int
    local_time: int
    device_os: str
    device_geo_city: str
    device_model: str
    bcat: str
    device_geo_zip: str
    user_yob: str
    app_publisher_name: str
    device_connection_type: int
    device_geo_region: str
    app_cat: str
    device_make: str
    device_hxw: str
    user_gender: int
    device_language: str
    imp_display_manager: str
    imp_banner_battr: str
    device_geo_country: str
    app_content_language: str
    app_content_context: int
    imp_banner_top_frame: bool
    device_type: int
    imp_instl: bool
    app_page_cat: str

    def __str__(self):
        return (
            f"BidRequest{{app_bundle='{self.app_bundle}', imp_banner_pos={self.imp_banner_pos}, "
            f"local_time={self.local_time}, device_os='{self.device_os}', "
            f"device_geo_city='{self.device_geo_city}', device_model='{self.device_model}', "
            f"bcat='{self.bcat}', device_geo_zip='{self.device_geo_zip}', user_yob='{self.user_yob}', "
            f"app_publisher_name='{self.app_publisher_name}', device_connection_type={self.device_connection_type}, "
            f"device_geo_region='{self.device_geo_region}', app_cat='{self.app_cat}', "
            f"device_make='{self.device_make}', device_hxw='{self.device_hxw}', user_gender={self.user_gender}, "
            f"device_language='{self.device_language}', imp_display_manager='{self.imp_display_manager}', "
            f"imp_banner_battr='{self.imp_banner_battr}', device_geo_country='{self.device_geo_country}', "
            f"app_content_language='{self.app_content_language}', app_content_context={self.app_content_context}, "
            f"imp_banner_top_frame={self.imp_banner_top_frame}, device_type={self.device_type}, "
            f"imp_instl={self.imp_instl}, app_page_cat='{self.app_page_cat}'}}"
        )

@dataclass
class Feedback:
    level1: bool
    level2: bool
    level3: bool

    def __str__(self):
        return (
            f"Feedback{{level1={self.level1}, level2={self.level2}, level3={self.level3}}}"
        )


def load_bid_requests(file_name: str) -> List[BidRequest]:
    if not os.path.exists(file_name):
        raise FileNotFoundError(f"File {file_name} not found.")

    with open(file_name, mode='r') as file:
        reader = csv.reader(file)
        bid_requests = []
        for record in reader:
            bid_requests.append(
                BidRequest(
                    app_bundle=record[0],
                    imp_banner_pos=int(record[1]),
                    local_time=int(record[2]),
                    device_os=record[3],
                    device_geo_city=record[4],
                    device_model=record[5],
                    bcat=record[6],
                    device_geo_zip=record[7],
                    user_yob=record[8],
                    app_publisher_name=record[9],
                    device_connection_type=int(record[10]),
                    device_geo_region=record[11],
                    app_cat=record[12],
                    device_make=record[13],
                    device_hxw=record[14],
                    user_gender=int(record[15]),
                    device_language=record[16],
                    imp_display_manager=record[17],
                    imp_banner_battr=record[18],
                    device_geo_country=record[19],
                    app_content_language=record[20],
                    app_content_context=int(record[21]),
                    imp_banner_top_frame=(record[22].lower() == "true"),
                    device_type=int(record[23]),
                    imp_instl=(record[24].lower() == "true"),
                    app_page_cat=record[25],
                )
            )
    return bid_requests


def load_feedbacks(file_name: str) -> List[Feedback]:
    if not os.path.exists(file_name):
        raise FileNotFoundError(f"File {file_name} not found.")

    with open(file_name, mode='r') as file:
        reader = csv.reader(file)
        feedbacks = []
        for record in reader:
            feedbacks.append(
                Feedback(
                    level1=(record[0].lower() == "true"),
                    level2=(record[1].lower() == "true"),
                    level3=(record[2].lower() == "true"),
                )
            )
    return feedbacks