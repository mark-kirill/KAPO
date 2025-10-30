import hmac
import hashlib
import requests
import urllib.parse

def test_all_possibilities():
    
    possible_keys = [
        "my_secret_key_123",
        "12345678", 
        "KAPOTeam",
        "admin123",
        "kapo_team",
        "66666666",
        "",
    ]
    
    test_message = "FF0000"  
    
    for key in possible_keys:
        print(f"\n--- Testing key: '{key}' ---")
        
        # Генерируем подпись
        if key:
            signature = hmac.new(
                key.encode('utf-8'),
                test_message.encode('utf-8'),
                hashlib.sha256
            ).hexdigest()
        else:
            signature = "no_key"
            
        print(f"Signature: {signature}")
        
        # URL encode the value parameter
        encoded_color = urllib.parse.quote(f"#{test_message}")
        url = f"http://192.168.1.154/set?value={encoded_color}&signature={signature}"
        
        try:
            response = requests.get(url, timeout=5)
            print(f"Status: {response.status_code}")
            if response.status_code == 200:
                print(f"✅ SUCCESS! Key found: '{key}'")
                return key
            elif "Invalid signature" in response.text:
                print("❌ Invalid signature")
            else:
                print(f"Response: {response.text}")
        except Exception as e:
            print(f"Error: {e}")
    
    return None


found_key = test_all_possibilities()
if found_key:
    print(f"\n🎉 Found the key: '{found_key}'")
else:
    print("\n🔴 Key not found in common defaults")