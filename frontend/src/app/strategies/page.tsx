"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { PageContainer } from "@/components/shared/PageContainer";
import { PageHeader } from "@/components/shared/PageHeader";
import { GlassCard, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/shared/GlassCard";
import { Switch } from "@/components/ui/switch";
import { Label } from "@/components/ui/label";
import { SlidersHorizontal } from "lucide-react";
import { useState } from "react";

export default function StrategiesPage() {
  const [latencyArbEnabled, setLatencyArbEnabled] = useState(true);
  const [dumpHedgeEnabled, setDumpHedgeEnabled] = useState(true);

  return (
    <DashboardLayout>
      <PageContainer>
        <PageHeader 
          title="Active Strategies" 
          description="Toggle automated detectors on or off. Changes will be pushed to the C++ core immediately."
          icon={SlidersHorizontal}
        />

        <GlassCard>
          <CardHeader>
            <CardTitle className="font-heading text-lg font-semibold tracking-tight text-gradient">Latency Arbitrage Detector</CardTitle>
            <CardDescription className="text-white/40 text-[13px] leading-relaxed">
              Exploits pricing delays between Binance (BTC tick) and Polymarket Order Book.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <Label htmlFor="latency-arb" className="flex flex-col space-y-1">
                <span className="font-semibold text-white/90 text-[14px]">Enable Detector</span>
                <span className="font-normal text-white/40 text-[12px] tracking-wide">
                  Runs evaluation every 50ms inside the execution loop.
                </span>
              </Label>
              <Switch 
                id="latency-arb" 
                checked={latencyArbEnabled}
                onCheckedChange={setLatencyArbEnabled}
              />
            </div>
            
            <div className="bg-white/5 p-4 rounded-xl border border-white/10">
              <h4 className="text-[11px] font-medium tracking-widest uppercase text-white/40 mb-3">Current Parameters</h4>
              <ul className="text-[13px] space-y-2.5">
                <li className="flex justify-between">
                  <span className="text-white/50">Asset</span>
                  <span className="font-mono text-white/90">BTC</span>
                </li>
                <li className="flex justify-between">
                  <span className="text-white/50">Minimum Edge</span>
                  <span className="font-mono text-white/90">0.040</span>
                </li>
              </ul>
            </div>
          </CardContent>
        </GlassCard>

        <GlassCard>
          <CardHeader>
            <CardTitle className="font-heading text-lg font-semibold tracking-tight text-gradient">Dump-Hedge Detector</CardTitle>
            <CardDescription className="text-white/40 text-[13px] leading-relaxed">
              Scans for order book illiquidity causing a combined Yes+No discount below 1.0.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <Label htmlFor="dump-hedge" className="flex flex-col space-y-1">
                <span className="font-semibold text-white/90 text-[14px]">Enable Detector</span>
                <span className="font-normal text-white/40 text-[12px] tracking-wide">
                  Searches for structural arbitrage opportunities inside the order book.
                </span>
              </Label>
              <Switch 
                id="dump-hedge" 
                checked={dumpHedgeEnabled}
                onCheckedChange={setDumpHedgeEnabled}
              />
            </div>

            <div className="bg-white/5 p-4 rounded-xl border border-white/10">
              <h4 className="text-[11px] font-medium tracking-widest uppercase text-white/40 mb-3">Current Parameters</h4>
              <ul className="text-[13px] space-y-2.5">
                <li className="flex justify-between">
                  <span className="text-white/50">Sum Target</span>
                  <span className="font-mono text-white/90">&lt; 0.95</span>
                </li>
                <li className="flex justify-between">
                  <span className="text-white/50">Minimum Discount</span>
                  <span className="font-mono text-white/90">0.03</span>
                </li>
              </ul>
            </div>
          </CardContent>
        </GlassCard>
      </PageContainer>
    </DashboardLayout>
  );
}
