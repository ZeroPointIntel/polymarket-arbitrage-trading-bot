import os
import time
import hmac
import hashlib
import base64
import requests
from dotenv import load_dotenv

def test_polymarket_auth():
    load_dotenv()
    
    api_key = os.getenv("POLY_API_KEY", "").strip()
    api_secret = os.getenv("POLY_API_SECRET", "").strip()
    api_passphrase = os.getenv("POLY_PASSPHRASE", "").strip()
    signer_address = os.getenv("POLYMARKET_SIGNER", "").strip()
    
    if not all([api_key, api_secret, api_passphrase, signer_address]):
        print("❌ Error: Missing credentials in .env file.")
        print(f"Key: {'SET' if api_key else 'MISSING'}")
        print(f"Secret: {'SET' if api_secret else 'MISSING'}")
        print(f"Passphrase: {'SET' if api_passphrase else 'MISSING'}")
        print(f"Signer: {'SET' if signer_address else 'MISSING'}")
        return

    # 1. TEST PUBLIC ENDPOINT FIRST (/time)
    host = "clob.polymarket.com"
    print(f"📡 Testing Public Access to {host}/time...")
    try:
        t_resp = requests.get(f"https://{host}/time", timeout=5)
        print(f"✅ Public /time: {t_resp.status_code}")
    except Exception as e:
        print(f"❌ Public /time Failed: {e}")

    # 2. TEST AUTHENTICATED ENDPOINT (/orders)
    path = "/orders" 
    method = "GET"
    timestamp = str(int(time.time()))
    message = timestamp + method + path
    
    # Robust decoding
    api_secret = api_secret.strip()
    try:
        api_secret = api_secret.replace('-', '+').replace('_', '/')
        while len(api_secret) % 4 != 0:
            api_secret += '='
        secret_bytes = base64.b64decode(api_secret)
    except Exception as e:
        print(f"❌ Error decoding API Secret: {e}")
        return
        
    signature_bytes = hmac.new(secret_bytes, message.encode('utf-8'), hashlib.sha256).digest()
    signature = base64.b64encode(signature_bytes).decode('utf-8')
    
    headers = {
        "POLY-API-KEY": api_key,
        "POLY-PASSPHRASE": api_passphrase,
        "POLY-TIMESTAMP": timestamp,
        "POLY-SIGNATURE": signature,
        "POLY-ADDRESS": signer_address.lower(),
        "User-Agent": "python-requests/2.31.0",
        "Accept": "application/json",
        "Content-Type": "application/json"
    }
    
    print(f"📡 Testing Authentication for {signer_address} via {path}...")
    try:
        response = requests.get(f"https://{host}{path}", headers=headers, timeout=10)
        if response.status_code == 200:
            print("\n✅ Authentication Successful!")
            print("-" * 40)
            print(f"Orders: {response.json()}")
            print("-" * 40)
        else:
            print(f"\n❌ Authentication Failed (Status {response.status_code})")
            print(f"Response: {response.text}")
            print("\nTIP: Double check your POLY_API_SECRET is the exact Base64 string from Polymarket.")
    except Exception as e:
        print(f"❌ Network error: {e}")

if __name__ == "__main__":
    test_polymarket_auth()
