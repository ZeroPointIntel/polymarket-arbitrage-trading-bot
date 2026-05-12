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

    # Endpoint to test (fetch allowance/balance info)
    host = "clob.polymarket.com"
    path = "/balance/allowance?asset_type=0" # 0 = USDC
    method = "GET"
    timestamp = str(int(time.time()))
    
    # Compute HMAC Signature
    # Payload: timestamp + method + path + body
    message = timestamp + method + path
    
    # Fix potential padding issues in the secret
    api_secret = api_secret.strip()
    print(f"🔍 DEBUG: Secret Length: {len(api_secret)}")
    
    # Robust decoding
    try:
        # Standardize to standard base64 if it's urlsafe
        api_secret = api_secret.replace('-', '+').replace('_', '/')
        
        # Add padding until it's a multiple of 4
        while len(api_secret) % 4 != 0:
            api_secret += '='
            
        print(f"🔍 DEBUG: Final Secret Length (with padding): {len(api_secret)}")
        secret_bytes = base64.b64decode(api_secret)
    except Exception as e:
        print(f"❌ Error decoding API Secret: {e}")
        return
        
    signature_bytes = hmac.new(secret_bytes, message.encode('utf-8'), hashlib.sha256).digest()
    signature = base64.b64encode(signature_bytes).decode('utf-8')
    
    headers = {
        "POLY_API_KEY": api_key,
        "POLY_PASSPHRASE": api_passphrase,
        "POLY_TIMESTAMP": timestamp,
        "POLY_SIGNATURE": signature,
        "POLY_ADDRESS": signer_address.lower()
    }
    
    print(f"📡 Testing Authentication for {signer_address}...")
    try:
        response = requests.get(f"https://{host}{path}", headers=headers)
        if response.status_code == 200:
            print("\n✅ Authentication Successful!")
            print("-" * 40)
            data = response.json()
            print(f"Response: {data}")
            print("-" * 40)
            print("\n🚀 You are ready to go live.")
        else:
            print(f"\n❌ Authentication Failed (Status {response.status_code})")
            print(f"Error: {response.text}")
            print("\nTIP: Double check your POLY_API_SECRET is the exact Base64 string from Polymarket.")
    except Exception as e:
        print(f"❌ Network error: {e}")

if __name__ == "__main__":
    test_polymarket_auth()
