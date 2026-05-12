"""
Fetch Polymarket balance using the official SDK.
Writes the balance to /tmp/poly_balance.txt for the C++ bot to read.
"""
import os
from dotenv import load_dotenv

def fetch_balance():
    load_dotenv()
    
    pk = os.getenv("POLYMARKET_PRIVATE_KEY", "").strip()
    funder = os.getenv("POLYMARKET_FUNDER", "").strip()
    
    if not pk:
        print("0.0")
        return
    
    if not pk.startswith("0x"):
        pk = "0x" + pk
    
    try:
        from py_clob_client.client import ClobClient
        from py_clob_client.constants import POLYGON
        
        host = "https://clob.polymarket.com"
        
        # Create client with proxy wallet support
        if funder:
            client = ClobClient(host, key=pk, chain_id=POLYGON, signature_type=1, funder=funder)
        else:
            client = ClobClient(host, key=pk, chain_id=POLYGON)
        
        # Derive API credentials
        creds = client.create_or_derive_api_creds()
        
        # Create authenticated client
        if funder:
            auth_client = ClobClient(host, key=pk, chain_id=POLYGON, signature_type=1, funder=funder, creds=creds)
        else:
            auth_client = ClobClient(host, key=pk, chain_id=POLYGON, creds=creds)
        
        # Try to get balance via the SDK
        try:
            bal = auth_client.get_balance_allowance()
            if isinstance(bal, dict) and 'balance' in bal:
                balance = float(bal['balance']) / 1e6  # USDC has 6 decimals
                print(f"{balance:.2f}")
                return
        except:
            pass
        
        # Fallback: try collateral balance
        try:
            bal = auth_client.get_collateral_balance()
            if bal is not None:
                balance = float(bal) / 1e6
                print(f"{balance:.2f}")
                return
        except:
            pass
        
        # If all methods fail, print 0
        print("0.0")
        
    except Exception as e:
        import sys
        print(f"0.0", file=sys.stdout)
        print(f"Error: {e}", file=sys.stderr)

if __name__ == "__main__":
    fetch_balance()
