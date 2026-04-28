"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Switch } from "@/components/ui/switch";
import { Label } from "@/components/ui/label";
import { useState } from "react";

export default function StrategiesPage() {
  const [latencyArbEnabled, setLatencyArbEnabled] = useState(true);
  const [dumpHedgeEnabled, setDumpHedgeEnabled] = useState(false);

  return (
    <DashboardLayout>
      <div className="space-y-6 max-w-4xl">
        <h1 className="text-3xl font-bold tracking-tight">Active Strategies</h1>
        <p className="text-muted-foreground">
          Toggle automated detectors on or off. Changes will be pushed to the C++ core immediately.
        </p>

        <Card>
          <CardHeader>
            <CardTitle>Latency Arbitrage Detector</CardTitle>
            <CardDescription>
              Exploits pricing delays between Binance (BTC tick) and Polymarket Order Book.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <Label htmlFor="latency-arb" className="flex flex-col space-y-1">
                <span>Enable Detector</span>
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
            
            <div className="bg-muted p-4 rounded-md">
              <h4 className="text-sm font-semibold mb-2">Current Parameters (Read-only)</h4>
              <ul className="text-sm space-y-1 text-muted-foreground">
                <li><span className="text-foreground">Asset:</span> BTC</li>
                <li><span className="text-foreground">Minimum Edge:</span> 0.040 (4 cents)</li>
              </ul>
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>Dump-Hedge Detector</CardTitle>
            <CardDescription>
              Scans for order book illiquidity causing a combined Yes+No discount below 1.0.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <Label htmlFor="dump-hedge" className="flex flex-col space-y-1">
                <span>Enable Detector</span>
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

            <div className="bg-muted p-4 rounded-md">
              <h4 className="text-sm font-semibold mb-2">Current Parameters (Read-only)</h4>
              <ul className="text-sm space-y-1 text-muted-foreground">
                <li><span className="text-foreground">Sum Target:</span> &lt; 0.95</li>
                <li><span className="text-foreground">Minimum Discount:</span> 0.03</li>
              </ul>
            </div>
          </CardContent>
        </Card>
      </div>
    </DashboardLayout>
  );
}
