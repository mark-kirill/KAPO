import hmac
import hashlib
import requests
import urllib.parse
import time

class SecureLEDClient:
    def __init__(self, base_url, secret_key):
        self.base_url = base_url
        self.secret_key = secret_key
        self.session = requests.Session()
        
    def login(self):
        login_data = {
            "username": "admin",
            "password": "1234"
        }
        
        try:
            response = self.session.post(
                f"{self.base_url}/login",
                data=login_data,
                allow_redirects=False  
            )
            
            if response.status_code == 302:  
                print("✅ Login successful")
                return True
            else:
                print(f"❌ Login failed: {response.status_code}")
                return False
                
        except Exception as e:
            print(f"Login error: {e}")
            return False
    
    def set_color(self, color_hex):
        signature = hmac.new(
            self.secret_key.encode('utf-8'),
            color_hex.encode('utf-8'),
            hashlib.sha256
        ).hexdigest()
        
        # URL encode
        encoded_color = urllib.parse.quote(color_hex)
        url = f"{self.base_url}/set?value={encoded_color}&signature={signature}"
        
        print(f"Setting color: {color_hex}")
        print(f"Signature: {signature}")
        
        try:
            response = self.session.get(url, timeout=10)
            print(f"Status: {response.status_code}")
            
            if response.status_code == 200:
                if "Color set to" in response.text:
                    print("🎉 Color changed successfully!")
                    time.sleep(0.3)
                    return True
                else:
                    print("⚠ Authenticated but unexpected response")
                    return False
            else:
                print(f"❌ Failed: {response.text[:100]}...")
                return False
                
        except Exception as e:
            print(f"Error: {e}")
            return False

if __name__ == "__main__":
    client = SecureLEDClient("http://192.168.1.154", "KAPOTeam")
    
    if client.login():
        colors = ["#FF0000", "#00FF00", "#0000FF", "#FFFFFF", "#FFA500"]
        
        for color in colors:
            success = client.set_color(color)
            
            if success:
                print(f"✅ {color} - OK")
            else:
                print(f"❌ {color} - Failed")
            print("---")
    else:
        print("❌ Cannot proceed without authentication")
