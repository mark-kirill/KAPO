import socket
import time


# Funktsioon UR5 ja M5Atom omavaheliseks suhtluseks
def handle_ur5_connection(ur5_socket, m5atom_socket):
    try:
        # Edastame requesti UR5-st M5Atomile (GET request)
        data = ur5_socket.recv(1024).decode().strip()
        print(f"Received from UR5: {data}")

        m5atom_socket.sendall(data.encode())
        print(f"Relayed request to M5Atom: {data}")

        # Saame vastust
        response = b""
        while True:
            part=m5atom_socket.recv(1024)
            response += part
            if not part: break
        
        response_str = response.decode("utf-8").strip()
        print(f"Received full response from M5Atom:\n{response_str}")

        # Send the M5Atom's response back to the UR5
        ur5_socket.sendall(response)
        print(f"Sent reply to UR5: {response}")

    finally:
        ur5_socket.close()

ur5_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ur5_server.bind(("0.0.0.0", 5000))  # Listen on all interfaces
ur5_server.listen(1)
print("Waiting for UR5 connection...")

while True:
    # Aktsepteerime ühendust UR5-ga
    ur5_conn, ur5_addr = ur5_server.accept()
    print(f"UR5 connected: {ur5_addr}")

    # Loome ühendust M5Atom's AP-iga 
    m5atom_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    m5atom_socket.connect(("192.168.4.1", 80))  
    print("Connected to M5Atom")

    handle_ur5_connection(ur5_conn, m5atom_socket)
