

import serial
import pynmea2
import datetime
from pydantic import BaseModel
import logging
from logging.handlers import RotatingFileHandler
from pathlib import Path

SERIAL_UART_PORT = '/dev/ttyAMA2'
BAUD = 38400
LOG_FILE = Path.home() / Path('gps_data.log')

# max log file size: 5 mb; keep 1 old log files
handler = RotatingFileHandler(LOG_FILE, maxBytes=5 * 1024 * 1024, backupCount=1, encoding='utf-8')
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s %(levelname)s %(name)s: %(message)s',
    handlers=[handler],
)
logger = logging.getLogger(__name__)


class GPSMainData(BaseModel):
    """GGA sentence-based data"""
    lat: float | None = None
    lon: float | None = None
    time: datetime.time | None = None
    altitude: float | None = None
    hdop: float | None = None
    satellites: int | None = None
    fix: bool


class GPSSecondaryData(BaseModel):
    """RMC sentence-based data"""
    lat: float | None = None
    lon: float | None = None
    time: datetime.time | None = None
    date: datetime.date | None = None
    speed: float | None = None
    fix: bool


class NMEABatchParsingError(Exception):
    ...


def nmea_coordinate_to_decimal(value: str, direction: str) -> float | None:
    """Convert an NMEA degrees-and-minutes coordinate to decimal degrees."""
    if value == '':
        return None

    degree_digits = 2 if direction in ('N', 'S') else 3
    degrees = int(value[:degree_digits])
    minutes = float(value[degree_digits:])
    coordinate = degrees + minutes / 60
    return -coordinate if direction in ('S', 'W') else coordinate


def parse_nmea_raw_sentences_batch(lines: list[bytes]) -> tuple[GPSMainData | None, GPSSecondaryData | None]:
    try:
        parsed_sentences = [pynmea2.parse(l_.decode('ascii')) for l_ in lines]
    except serial.SerialException as e:
        raise NMEABatchParsingError(f'Device error: {e}')
    except pynmea2.ParseError as e:
        raise NMEABatchParsingError(f'Parse error: {e}')
    except UnicodeDecodeError as e:
        raise NMEABatchParsingError(f'Decode error: {e}, line bytes: {repr(lines)}')

    # for ps in parsed_sentences:
    #     print('Parsed sentence:', repr(ps))

    gga_sentence = next((s for s in parsed_sentences if s.sentence_type == 'GGA'), None)
    if gga_sentence is not None:
        main_data = GPSMainData(
            lat=nmea_coordinate_to_decimal(gga_sentence.lat, gga_sentence.lat_dir),
            lon=nmea_coordinate_to_decimal(gga_sentence.lon, gga_sentence.lon_dir),
            time=gga_sentence.timestamp,
            altitude=gga_sentence.altitude,
            hdop=gga_sentence.horizontal_dil,
            satellites=gga_sentence.num_sats,
            fix=gga_sentence.gps_qual > 0,
        )
    else:
        main_data = None

    rmc_sentence = next((s for s in parsed_sentences if s.sentence_type == 'RMC'), None)

    if rmc_sentence is not None:
        secondary_data = GPSSecondaryData(
            lat=nmea_coordinate_to_decimal(rmc_sentence.lat, rmc_sentence.lat_dir),
            lon=nmea_coordinate_to_decimal(rmc_sentence.lon, rmc_sentence.lon_dir),
            time=rmc_sentence.timestamp,
            date=rmc_sentence.datestamp,
            speed=rmc_sentence.spd_over_grnd,
            fix=rmc_sentence.status == 'A',
        )
    else:
        secondary_data = None

    return main_data, secondary_data


if __name__ == '__main__':
    ser = serial.Serial(SERIAL_UART_PORT, BAUD, timeout=5.0)
    line_bytes = b''
    ser.reset_input_buffer()
    while True:
        lines = [ser.readline()]
        while ser.in_waiting > 0:
            lines.append(ser.readline())
        try:
            gps_main_data, gps_secondary_data = parse_nmea_raw_sentences_batch(lines)
        except NMEABatchParsingError:
            logger.exception('Batch parsing error')
            gps_main_data, gps_secondary_data = None, None

        if gps_main_data is not None:
            logger.info('record=main,' + ','.join(f'{k}={v}' for k, v in gps_main_data.model_dump().items()))
        if gps_secondary_data is not None:
            logger.info('record=secondary,' + ','.join(f'{k}={v}' for k, v in gps_secondary_data.model_dump().items()))
