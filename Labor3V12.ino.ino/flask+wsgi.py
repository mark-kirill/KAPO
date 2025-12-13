from flask import Flask, render_template,redirect, jsonify, request, Response, url_for, session
import json
import os
import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient
from datetime import datetime, timezone, timedelta
import threading
import time
import csv
from io import StringIO

app = Flask(__name__)

#  JSON 
DEVICES_FILE = '/var/www/webApp/devices.json'
MOSQUITTO_DB = '/var/lib/mosquitto/mosquitto.db'

app.secret_key = "KAPOTeamPoweredByBOSCH!!!%%%"  

USERS = {
    "student": "KAPOTeam!",
    "asdabvasd": "sdava@#532!@"
}

# InfluxDB client
influx_client = InfluxDBClient(
    host='localhost',
    port=8086,
    username='admin',
    password='KAPOTeam!',
    database='esp32valve'
)

# stats
device_stats = {}  #device stats
last_messages = {}  # buffer last message
BLOCKED_DEVICES = ['atomm5']  


def load_devices():
    try:
        with open(DEVICES_FILE) as f:
            return json.load(f)
    except FileNotFoundError:
        return {}


def save_devices(devices):
    with open(DEVICES_FILE, 'w') as f:
        json.dump(devices, f, indent=4)

# MQTT callback
def on_connect(client, userdata, flags, rc):
    print(f"✅ Connected to MQTT with result code {rc}")
    client.subscribe("sensors/+/data")
    client.subscribe("sensors/+/init")  

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    device_id = msg.topic.split('/')[1]
    topic_type = msg.topic.split('/')[2]  # data или init

    # blocks
    if device_id in BLOCKED_DEVICES:
        print(f"🚫 BLOCKED: Ignoring message from {device_id}")
        return

    #  init 
    if topic_type == 'init':
        devices = load_devices()
        if device_id not in devices:
            devices[device_id] = {}
        devices[device_id]['html_fragment'] = payload
        devices[device_id]['description'] = f"Valve Device {device_id}"
        save_devices(devices)
        print(f"📝 Saved HTML fragment for {device_id}")
        return

    #  data 
    now = datetime.now(timezone.utc)
    now_str = now.isoformat()

    # last mess in buff
    last_messages[device_id] = {
        'payload': payload,
        'timestamp': now
    }

    # update devices.json
    devices = load_devices()
    if device_id not in devices:
        devices[device_id] = {"description": f"Device {device_id}"}
    devices[device_id]['last_seen'] = now_str
    save_devices(devices)

    # parsing JSON 
    try:
        data = json.loads(payload)

        
        if device_id not in device_stats:
            device_stats[device_id] = {
                'total_records': 0,
                'total_batches': 0,
                'last_batch_size': 0,
                'last_write': time.time()
            }

       
        records_to_write = []

        if 'batch' in data or 'data' in data:
            # {"uid":"...", "count":50, "data":[...]}
            batch_data = data.get('data', data.get('batch', []))
            device_stats[device_id]['total_batches'] += 1
            device_stats[device_id]['last_batch_size'] = len(batch_data)

            print(f"📦 Batch from {device_id}: {len(batch_data)} records")

            for record in batch_data:
                # format: {"t":timestamp, "v":0/1, "p":pressure}
                timestamp_ms = record.get('t', 0)
                valve_open = record.get('v', 0)
                pressure = record.get('p', 0.0)

                if timestamp_ms < 1000000000000:  # Меньше 2001 года
                    record_time = now  # Тестовый режим
                else:
                    record_time = datetime.fromtimestamp(timestamp_ms / 1000.0, tz=timezone.utc)

                records_to_write.append({
                    "measurement": "device_data",
                    "tags": {"device": device_id},
                    "fields": {
                        "pressure_now": float(pressure),
                        "valve_state": "open" if valve_open else "closed"
                    },
                    "time": record_time.isoformat()
                })

        else:
            pressure_now = float(data.get('pressure_now', 0))
            pressure_prev = float(data.get('pressure_30ms_ago', 0))
            valve_state = data.get('valve_state', 'unknown')

            device_stats[device_id]['last_batch_size'] = 1
            print(f"📊 Single record from {device_id}")

            records_to_write.append({
                "measurement": "device_data",
                "tags": {"device": device_id},
                "fields": {
                    "pressure_now": pressure_now,
                    "pressure_prev": pressure_prev,
                    "valve_state": valve_state
                },
                "time": now_str
            })

        #  InfluxDB
        if records_to_write:
            influx_client.write_points(records_to_write, batch_size=100)
            device_stats[device_id]['total_records'] += len(records_to_write)
            device_stats[device_id]['last_write'] = time.time()

            print(f"✅ Wrote {len(records_to_write)} records to InfluxDB for {device_id}")
            print(f"   Total: {device_stats[device_id]['total_records']} records, "
                  f"{device_stats[device_id]['total_batches']} batches")

    except json.JSONDecodeError as e:
        print(f"❌ Invalid JSON from {device_id}: {e}")
        print(f"   Payload: {payload[:100]}...")
    except Exception as e:
        print(f"❌ InfluxDB write error for {device_id}: {e}")
        import traceback
        traceback.print_exc()

# run MQTT in sub
def mqtt_thread():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.username_pw_set("mqtt_user", "KAPOTeam!")
    client.connect("localhost", 1883, 60)
    client.loop_forever()

threading.Thread(target=mqtt_thread, daemon=True).start()

def login_required(func):
    """Декоратор, чтобы защитить страницы паролем"""
    from functools import wraps
    @wraps(func)
    def wrapper(*args, **kwargs):
        if 'username' not in session:
            return redirect(url_for('login'))
        return func(*args, **kwargs)
    return wrapper


def relative_time(last_seen_str):
    last_seen = datetime.fromisoformat(last_seen_str)
    now = datetime.now(timezone.utc)
    delta = now - last_seen.replace(tzinfo=timezone.utc)
    minutes = int(delta.total_seconds() // 60)
    if minutes == 0:
        return "Just now"
    elif minutes == 1:
        return "1 min ago"
    else:
        return f"{minutes} min ago"


@app.route("/login", methods=["GET", "POST"])
def login():
    error = ""
    if request.method == "POST":
        username = request.form.get("username")
        password = request.form.get("password")
        if username in USERS and USERS[username] == password:
            session['username'] = username
            return redirect(url_for("index"))
        else:
            error = "Vale kasutajanimi või parool"
    return render_template("login.html", error=error)

@app.route("/logout")
def logout():
    session.pop('username', None)
    return redirect(url_for("login"))

@app.route('/')
@login_required
def index():
    devices = load_devices()
    for dev_id, dev in devices.items():
        if 'last_seen' in dev:
            dev['relative_time'] = relative_time(dev['last_seen'])
            delta = (datetime.now(timezone.utc) -
                    datetime.fromisoformat(dev['last_seen']).replace(tzinfo=timezone.utc)).total_seconds()
            dev['status'] = 'Online' if delta < 300 else 'Offline'
        else:
            dev['relative_time'] = 'unknown'
            dev['status'] = 'Unknown'

        
        if dev_id in device_stats:
            dev['stats'] = device_stats[dev_id]
        else:
            dev['stats'] = {
                'total_records': 0,
                'total_batches': 0,
                'last_batch_size': 0
            }

    return render_template('index.html', devices=devices)

@app.route('/device/<uid>/history')
@login_required
def history(uid):
    """История последних 1000 записей (увеличено для батчей)"""
    result = influx_client.query(
        f'SELECT * FROM device_data WHERE device=\'{uid}\' ORDER BY time DESC LIMIT 1000'
    )
    points = list(result.get_points())
    return jsonify(points)

@app.route('/device/<uid>/history_24h')
@login_required
def history_24h(uid):
    """История за последние 24 часа"""
    result = influx_client.query(
        f"SELECT * FROM device_data WHERE device='{uid}' "
        f"AND time > now() - 24h ORDER BY time DESC LIMIT 2000"
    )
    points = list(result.get_points())
    return jsonify(points)

@app.route('/device/<uid>/valve_history')
@login_required
def valve_history(uid):
    """История состояния клапана для отдельного графика"""
    result = influx_client.query(
        f'SELECT valve_state, time FROM device_data '
        f'WHERE device=\'{uid}\' ORDER BY time DESC LIMIT 100'
    )
    points = list(result.get_points())
    points.reverse()

    for point in points:
        point['valve_numeric'] = 1 if point.get('valve_state') == 'open' else 0
    return jsonify(points)

@app.route('/device/<uid>/stats')
@login_required
def device_stats_endpoint(uid):
    """Подробная статистика по устройству"""
    if uid in device_stats:
        stats = device_stats[uid].copy()
        stats['device'] = uid
        stats['seconds_since_last_write'] = time.time() - stats['last_write']

        #  info from devices.json
        devices = load_devices()
        if uid in devices and 'last_seen' in devices[uid]:
            stats['last_seen'] = devices[uid]['last_seen']

        return jsonify(stats)

    return jsonify({'error': 'Device not found'}), 404

@app.route('/device/<uid>/realtime')
@login_required
def realtime_data(uid):
    """Последние 100 точек для графика реального времени (давление и состояние клапана)"""
    result = influx_client.query(
        f'SELECT pressure_now, valve_state FROM device_data '
        f'WHERE device=\'{uid}\' ORDER BY time DESC LIMIT 100'
    )
    points = list(result.get_points())
    # Разворачиваем для правильного порядка на графике
    points.reverse()
    return jsonify(points)

@app.route('/device/<uid>/export', methods=['GET', 'POST'])
@login_required
def export_data(uid):
    """Экспорт данных по временному диапазону с HTML формой"""
    if request.method == 'POST':
        # Получаем данные из формы
        format_type = request.form.get('format', 'json')
        time_range = request.form.get('time_range', 'last_hour')

        # Устанавливаем временной диапазон
        end_time = datetime.now(timezone.utc)

        if time_range == 'last_hour':
            start_time = end_time - timedelta(hours=1)
        elif time_range == 'last_24h':
            start_time = end_time - timedelta(days=1)
        elif time_range == 'last_week':
            start_time = end_time - timedelta(weeks=1)
        elif time_range == 'last_month':
            start_time = end_time - timedelta(days=30)
        elif time_range == 'custom':
            start_str = request.form.get('custom_start')
            end_str = request.form.get('custom_end')
            if start_str:
                start_time = datetime.fromisoformat(start_str)
            else:
                start_time = end_time - timedelta(days=1)
            if end_str:
                end_time = datetime.fromisoformat(end_str)
        else:
            start_time = end_time - timedelta(hours=1)

        limit = int(request.form.get('limit', 10000))

        # Формируем запрос к InfluxDB
        query = f'SELECT * FROM device_data WHERE device=\'{uid}\''

        # Добавляем временной диапазон
        query += f" AND time >= '{start_time.isoformat()}'"
        query += f" AND time <= '{end_time.isoformat()}'"

        query += f' ORDER BY time DESC LIMIT {limit}'

        result = influx_client.query(query)
        points = list(result.get_points())

        if format_type == 'csv':
            # Создаем CSV
            if not points:
                return "No data found", 404

            # Создаем CSV строку
            output = StringIO()
            writer = csv.DictWriter(output, fieldnames=points[0].keys())
            writer.writeheader()
            writer.writerows(points)

            # Возвращаем CSV файл
            filename = f"{uid}_export_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
            response = Response(
                output.getvalue(),
                mimetype='text/csv',
                headers={'Content-Disposition': f'attachment; filename={filename}'}
            )
            return response
        else:
            # Возвращаем JSON файл
            filename = f"{uid}_export_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            response = jsonify(points)
            response.headers['Content-Disposition'] = f'attachment; filename={filename}'
            return response

    # GET запрос - показываем форму экспорта
    return render_template('export_form.html', device_id=uid)

@app.route('/device/<uid>/quick_export/<time_range>')
@login_required
def quick_export(uid, time_range):
    """Быстрый экспорт по предустановленным диапазонам"""
    end_time = datetime.now(timezone.utc)

    if time_range == 'last_hour':
        start_time = end_time - timedelta(hours=1)
        limit = 1000
    elif time_range == 'last_24h':
        start_time = end_time - timedelta(days=1)
        limit = 5000
    elif time_range == 'last_week':
        start_time = end_time - timedelta(weeks=1)
        limit = 20000
    else:
        start_time = end_time - timedelta(hours=1)
        limit = 1000

    format_type = 'csv'

    # Формируем запрос к InfluxDB
    query = f'SELECT * FROM device_data WHERE device=\'{uid}\''
    query += f" AND time >= '{start_time.isoformat()}'"
    query += f" AND time <= '{end_time.isoformat()}'"
    query += f' ORDER BY time DESC LIMIT {limit}'

    result = influx_client.query(query)
    points = list(result.get_points())

    if not points:
        return "No data found", 404

    # Создаем CSV
    output = StringIO()
    writer = csv.DictWriter(output, fieldnames=points[0].keys())
    writer.writeheader()
    writer.writerows(points)

    # Возвращаем CSV файл
    filename = f"{uid}_{time_range}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    response = Response(
        output.getvalue(),
        mimetype='text/csv',
        headers={'Content-Disposition': f'attachment; filename={filename}'}
    )
    return response

@app.route('/stats/all')
@login_required
def all_stats():
    """Общая статистика по всем устройствам"""
    summary = {
        'total_devices': len(device_stats),
        'total_records': sum(s['total_records'] for s in device_stats.values()),
        'total_batches': sum(s['total_batches'] for s in device_stats.values()),
        'devices': {}
    }

    for dev_id, stats in device_stats.items():
        summary['devices'][dev_id] = {
            'records': stats['total_records'],
            'batches': stats['total_batches'],
            'avg_batch_size': stats['total_records'] / max(stats['total_batches'], 1),
            'last_batch': stats['last_batch_size'],
            'seconds_since_write': time.time() - stats['last_write']
        }

    return jsonify(summary)

if __name__ == '__main__':
    print("🚀 Starting Flask app with batch MQTT support...")
    print("📊 Subscribed to: sensors/+/data and sensors/+/init")
    app.run(host='0.0.0.0', port=5000, debug=False)