import requests
import asyncio
import os
from web3 import Web3
from config import BotConfig
from core.polymarket_client import PolymarketClient
from py_clob_client.clob_types import BalanceAllowanceParams, AssetType

async def main():
    config = BotConfig()
    pm = PolymarketClient(
        host=config.polymarket_host,
        chain_id=config.polymarket_chain_id,
        private_key=config.polymarket_private_key,
        funder=config.polymarket_funder,
        signature_type=config.polymarket_signature_type,
        paper_mode=False
    )
    eoa_address = pm._client.get_address()
    funder_address = config.polymarket_funder

    print("=== Polymarket Client Configuration ===")
    print("Signature Type:", config.polymarket_signature_type)
    print("EOA Address:", eoa_address)
    print("Funder (Proxy) Address:", funder_address)

    # 1. Test Py-Clob-Client
    try:
        print("\n=== Py-Clob-Client Balance API ===")
        params = BalanceAllowanceParams(asset_type=AssetType.COLLATERAL)
        data = pm._client.get_balance_allowance(params=params)
        print("API Collateral Balance:", float(data.get("balance", 0)) / 1e6, "USDC")
    except Exception as e:
        print("API Error:", e)

    # 2. Test On-Chain (Web3)
    print("\n=== On-Chain Polygon Balances (Web3) ===")
    w3 = Web3(Web3.HTTPProvider("https://polygon.llamarpc.com"))
    if not w3.is_connected():
        w3 = Web3(Web3.HTTPProvider("https://polygon.drpc.org"))

    usdc_abi = [{"constant":True,"inputs":[{"name":"_owner","type":"address"}],"name":"balanceOf","outputs":[{"name":"balance","type":"uint256"}],"type":"function"}]
    usdc_e_contract = w3.eth.contract(address="0x2791Bca1f2de4661ED88A30C99A7a9449Aa84174", abi=usdc_abi)
    usdc_native_contract = w3.eth.contract(address="0x3c499c542cEF5E3811e1192ce70d8cC03d5c3359", abi=usdc_abi)

    def print_balances(label, address):
        if not address: return
        addr = w3.to_checksum_address(address)
        matic = w3.eth.get_balance(addr) / 1e18
        usdc_e = usdc_e_contract.functions.balanceOf(addr).call() / 1e6
        usdc_native = usdc_native_contract.functions.balanceOf(addr).call() / 1e6
        print(f"[{label}] MATIC:       {matic:.4f}")
        print(f"[{label}] USDC.e:      {usdc_e:.6f}")
        print(f"[{label}] USDC Native: {usdc_native:.6f}")

    print_balances("EOA Wallet", eoa_address)
    print_balances("Proxy Wallet", funder_address)

if __name__ == "__main__":
    asyncio.run(main())
