import hmac
import hashlib
import requests
import urllib.parse
import time
import statistics

class SecureLEDClient:
    def __init__(self, base_url, secret_key):
        self.base_url = base_url
        self.secret_key = secret_key
        self.session = requests.Session()
        self.response_times = []
        
    def login(self):
        login_data = {
            "username": "admin",
            "password": "1234"
        }
        
        try:
            start_time = time.time()
            response = self.session.post(
                f"{self.base_url}/login",
                data=login_data,
                allow_redirects=False
            )
            login_time = (time.time() - start_time) * 1000
            
            if response.status_code == 302:
                print(f"✅ Login successful ({login_time:.1f}ms)")
                return True
            else:
                print(f"❌ Login failed: {response.status_code}")
                return False
                
        except Exception as e:
            print(f"Login error: {e}")
            return False
    
    def set_color(self, color_hex, delay_after=0.5):
        message = color_hex  # "#FF0000"
        
        start_time = time.time()
        
        signature = hmac.new(
            self.secret_key.encode('utf-8'),
            message.encode('utf-8'),
            hashlib.sha256
        ).hexdigest()
        
        encoded_message = urllib.parse.quote(message)
        url = f"{self.base_url}/set?value={encoded_message}&signature={signature}"
        
        print(f"🎨 Setting color: {color_hex}")
        print(f"🔐 Signature: {signature[:16]}...")
        
        try:
            response = self.session.get(url, timeout=10)
            response_time = (time.time() - start_time) * 1000
            self.response_times.append(response_time)
            
            print(f"📊 Status: {response.status_code} ({response_time:.1f}ms)")
            
            if response.status_code == 200:
                if "Color set to" in response.text:
                    print("✅ Color changed successfully!")
                    if delay_after > 0:
                        time.sleep(delay_after)
                    return True
                else:
                    print("⚠ Authenticated but unexpected response")
                    return False
            else:
                print(f"❌ Failed: {response.text[:100]}...")
                return False
                
        except Exception as e:
            print(f"💥 Error: {e}")
            return False

    def test_hmac_verification(self):
        print("\n" + "="*50)
        print("🔐 TESTING HMAC VERIFICATION")
        print("="*50)
        
        test_cases = [
            # (color, expected_result, description)
            ("#FF0000", True, "Valid color with correct key"),
            ("#00FF00", True, "Another valid color"),
            ("#0000FF", True, "Third valid color"),
        ]
        
        results = []
        
        for color, expected, description in test_cases:
            print(f"\n🧪 Test: {description}")
            print(f"   Color: {color}")
            
            success = self.set_color(color, delay_after=1.0)
            
            if success == expected:
                results.append(True)
                print("   ✅ PASS")
            else:
                results.append(False)
                print("   ❌ FAIL")
        
        print(f"\n🧪 Test: Invalid signature (wrong key)")
        wrong_key = "WRONG_KEY_123"
        message = "#FF0000"
        
        wrong_signature = hmac.new(
            wrong_key.encode('utf-8'),
            message.encode('utf-8'),
            hashlib.sha256
        ).hexdigest()
        
        encoded_message = urllib.parse.quote(message)
        url = f"{self.base_url}/set?value={encoded_message}&signature={wrong_signature}"
        
        try:
            response = self.session.get(url, timeout=5)
            if response.status_code == 401:
                results.append(True)
                print("   ✅ PASS - correctly rejected invalid signature")
            else:
                results.append(False)
                print(f"   ❌ FAIL - should reject but got {response.status_code}")
        except Exception as e:
            print(f"   ❌ FAIL - Error: {e}")
            results.append(False)
        
        passed = sum(results)
        total = len(results)
        print(f"\n📊 HMAC Verification Results: {passed}/{total} passed")
        
        return all(results)

    def test_performance(self, num_requests=10):
        print("\n" + "="*50)
        print("⚡ PERFORMANCE TEST")
        print("="*50)
        
        print(f"Running {num_requests} requests to measure performance...")
        
        self.response_times = []
        
        test_color = "#FFA500" 
        
        for i in range(num_requests):
            print(f"\n📨 Request {i+1}/{num_requests}")
            success = self.set_color(test_color, delay_after=0.1)
            
            if not success:
                print("❌ Performance test failed - stopping")
                return False
        
        if self.response_times:
            avg_time = statistics.mean(self.response_times)
            min_time = min(self.response_times)
            max_time = max(self.response_times)
            std_dev = statistics.stdev(self.response_times) if len(self.response_times) > 1 else 0
            
            print(f"\n📊 PERFORMANCE RESULTS:")
            print(f"   Average response time: {avg_time:.1f}ms")
            print(f"   Minimum response time: {min_time:.1f}ms")
            print(f"   Maximum response time: {max_time:.1f}ms")
            print(f"   Standard deviation: {std_dev:.1f}ms")
            print(f"   Sample size: {len(self.response_times)} requests")
            
            recommended_window = avg_time * 5  
            print(f"   💡 Recommended time window: {recommended_window:.0f}ms")
            
            if avg_time < 100:
                rating = "EXCELLENT 🚀"
            elif avg_time < 500:
                rating = "GOOD 👍"
            elif avg_time < 1000:
                rating = "ACCEPTABLE ⚠️"
            else:
                rating = "SLOW 🐢"
                
            print(f"   Performance rating: {rating}")
            
            return True
        else:
            print("❌ No response times recorded")
            return False

    def color_sequence_test(self):
        print("\n" + "="*50)
        print("🎨 COLOR SEQUENCE TEST")
        print("="*50)
        
        colors = [
            ("#FF0000", "Red", 1.0),
            ("#00FF00", "Green", 1.0),
            ("#0000FF", "Blue", 1.0),
            ("#FFFF00", "Yellow", 0.5),
            ("#FF00FF", "Magenta", 0.5),
            ("#00FFFF", "Cyan", 0.5),
            ("#FFFFFF", "White", 1.0),
            ("#000000", "Black", 1.0),
        ]
        
        success_count = 0
        
        for color, name, delay in colors:
            print(f"\n🎨 {name} {color}")
            if self.set_color(color, delay_after=delay):
                success_count += 1
            else:
                print(f"❌ Failed to set {name}")
        
        print(f"\n📊 Color Sequence Results: {success_count}/{len(colors)} successful")
        return success_count == len(colors)

    def run_complete_test_suite(self):
        print("🚀 STARTING COMPLETE TEST SUITE")
        print("="*60)
        
        if not self.login():
            print("❌ Cannot proceed without authentication")
            return False

        hmac_success = self.test_hmac_verification()
        

        performance_success = self.test_performance(num_requests=8)
        

        color_success = self.color_sequence_test()
        

        print("\n" + "="*60)
        print("🎯 FINAL TEST RESULTS")
        print("="*60)
        print(f"🔐 HMAC Verification: {'✅ PASS' if hmac_success else '❌ FAIL'}")
        print(f"⚡ Performance Test: {'✅ PASS' if performance_success else '❌ FAIL'}")
        print(f"🎨 Color Sequence: {'✅ PASS' if color_success else '❌ FAIL'}")
        
        if hmac_success and performance_success and color_success:
            print("\n🎉 ALL TESTS PASSED! System is working correctly. 🎉")
            return True
        else:
            print("\n💥 SOME TESTS FAILED! Check the system. 💥")
            return False

def quick_demo():
    print("🎪 QUICK DEMO MODE")
    client = SecureLEDClient("http://192.168.1.154", "KAPOTeam")
    
    if client.login():
        demo_sequence = [
            ("#FF0000", "Red", 2.0),
            ("#00FF00", "Green", 1.5),
            ("#0000FF", "Blue", 2.0),
            ("#FFFF00", "Yellow", 1.0),
            ("#FF00FF", "Pink", 1.0),
            ("#00FFFF", "Cyan", 1.5),
            ("#FFFFFF", "White", 1.0),
        ]
        
        for color, name, delay in demo_sequence:
            print(f"\n🎨 {name}")
            client.set_color(color, delay_after=delay)
        
        print("\n✨ Demo completed!")

if __name__ == "__main__":
    print("M5Atom Secure LED Control - Test Suite")
    print("IP: 192.168.1.154 | Key: KAPOTeam")
    print()
    
    client = SecureLEDClient("http://192.168.1.154", "KAPOTeam")
    
    while True:
        print("\n" + "="*50)
        print("Choose test mode:")
        print("1. 🚀 Full Test Suite (HMAC + Performance + Colors)")
        print("2. 🔐 HMAC Verification Test Only")
        print("3. ⚡ Performance Test Only")  
        print("4. 🎨 Color Sequence Test Only")
        print("5. 🎪 Quick Demo")
        print("6. ❌ Exit")
        
        choice = input("\nEnter choice (1-6): ").strip()
        
        if choice == "1":
            client.run_complete_test_suite()
        elif choice == "2":
            if client.login():
                client.test_hmac_verification()
        elif choice == "3":
            if client.login():
                client.test_performance()
        elif choice == "4":
            if client.login():
                client.color_sequence_test()
        elif choice == "5":
            quick_demo()
        elif choice == "6":
            print("👋 Goodbye!")
            break
        else:
            print("❌ Invalid choice")
