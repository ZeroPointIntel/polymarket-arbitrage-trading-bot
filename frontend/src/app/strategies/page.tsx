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
  const [dumpHedgeEnabled, setDumpHedgeEnabled] = useState(false);

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
            <CardTitle className="text-primary">Latency Arbitrage Detector</CardTitle>
            <CardDescription>
              Exploits pricing delays between Binance (BTC tick) and Polymarket Order Book.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <Label htmlFor="latency-arb" className="flex flex-col space-y-1">
                <span className="font-semibold">Enable Detector</span>
                <span className="font-normal text-muted-foreground text-xs">
                  Runs evaluation every 50ms inside the execution loop.
                </span>
              </Label>
              <Switch 
                id="latency-arb" 
                checked={latencyArbEnabled}
                onCheckedChange={setLatencyArbEnabled}
              />
            </div>
            
            <div className="bg-black/20 p-4 rounded-md border border-white/5">
              <h4 className="text-sm font-semibold mb-2">Current Parameters (Read-only)</h4>
              <ul className="text-sm space-y-2 text-muted-foreground">
                <li className="flex justify-between"><span className="text-white">Asset:</span> BTC</li>
                <li className="flex justify-between"><span className="text-white">Minimum Edge:</span> 0.040 (4 cents)</li>
              </ul>
            </div>
          </CardContent>
        </GlassCard>

        <GlassCard>
          <CardHeader>
            <CardTitle className="text-primary">Dump-Hedge Detector</CardTitle>
            <CardDescription>
              Scans for order book illiquidity causing a combined Yes+No discount below 1.0.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <Label htmlFor="dump-hedge" className="flex flex-col space-y-1">
                <span className="font-semibold">Enable Detector</span>
                <span className="font-normal text-muted-foreground text-xs">
                  Searches for structural arbitrage opportunities inside the order book.
                </span>
              </Label>
              <Switch 
                id="dump-hedge" 
                checked={dumpHedgeEnabled}
                onCheckedChange={setDumpHedgeEnabled}
              />
            </div>

            <div className="bg-black/20 p-4 rounded-md border border-white/5">
              <h4 className="text-sm font-semibold mb-2">Current Parameters (Read-only)</h4>
              <ul className="text-sm space-y-2 text-muted-foreground">
                <li className="flex justify-between"><span className="text-white">Sum Target:</span> &lt; 0.95</li>
                <li className="flex justify-between"><span className="text-white">Minimum Discount:</span> 0.03</li>
              </ul>
            </div>
          </CardContent>
        </GlassCard>
      </PageContainer>
    </DashboardLayout>
  );
}
