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
        return

    host = "clob.polymarket.com"
    
    # 1. TEST PUBLIC
    print(f"📡 Testing Public Access to {host}/time...")
    try:
        t_resp = requests.get(f"https://{host}/time", timeout=5)
        print(f"✅ Public /time: {t_resp.status_code}")
    except Exception as e:
        print(f"❌ Public /time Failed: {e}")

    # 2. TEST AUTHENTICATED ENDPOINT (/orders)
    path = "/orders"
    print(f"\n📡 Testing Authentication via {path}...")
    method = "GET"
    timestamp = str(int(time.time()))
    nonce = "0" # Some versions require nonce even on GET
    message = timestamp + method + path + nonce
    
    # Robust decoding
    api_secret_clean = api_secret.strip().replace('-', '+').replace('_', '/')
    while len(api_secret_clean) % 4 != 0:
        api_secret_clean += '='
    secret_bytes = base64.b64decode(api_secret_clean)
        
    signature_bytes = hmac.new(secret_bytes, message.encode('utf-8'), hashlib.sha256).digest()
    signature = base64.b64encode(signature_bytes).decode('utf-8')
    
    # Try Hyphens first, then Underscores if fails
    header_sets = [
        {
            "POLY-API-KEY": api_key,
            "POLY-PASSPHRASE": api_passphrase,
            "POLY-TIMESTAMP": timestamp,
            "POLY-SIGNATURE": signature,
            "POLY-ADDRESS": signer_address.lower(),
            "POLY-NONCE": nonce
        },
        {
            "POLY_API_KEY": api_key,
            "POLY_PASSPHRASE": api_passphrase,
            "POLY_TIMESTAMP": timestamp,
            "POLY_SIGNATURE": signature,
            "POLY_ADDRESS": signer_address.lower(),
            "POLY_NONCE": nonce
        }
    ]
    
    for headers in header_sets:
        style = "Hyphens" if "-" in list(headers.keys())[0] else "Underscores"
        print(f"--- Trying {style} ---")
        headers.update({
            "User-Agent": "python-requests/2.31.0",
            "Accept": "application/json",
            "Content-Type": "application/json"
        })
        
        try:
            response = requests.get(f"https://{host}{path}", headers=headers, timeout=10)
            print(f"Result: Status {response.status_code}")
            if response.status_code == 200:
                print(f"✅ SUCCESS with {style}!")
                print(f"Response: {response.json()}")
                return
            else:
                print(f"❌ Failed: {response.text}")
        except Exception as e:
            print(f"❌ Error: {e}")

if __name__ == "__main__":
    test_polymarket_auth()
