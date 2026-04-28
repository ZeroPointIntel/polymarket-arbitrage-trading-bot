"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { Card, CardContent, CardHeader, CardTitle, CardDescription, CardFooter } from "@/components/ui/card";
import { Slider } from "@/components/ui/slider";
import { Label } from "@/components/ui/label";
import { Button } from "@/components/ui/button";
import { useState } from "react";
import { ShieldAlert } from "lucide-react";

export default function RiskPage() {
  const [maxPosition, setMaxPosition] = useState([8]); // 8%
  const [dailyLoss, setDailyLoss] = useState([20]); // 20%
  const [drawdownKill, setDrawdownKill] = useState([40]); // 40%
  const [loading, setLoading] = useState(false);

  const handleSave = async () => {
    setLoading(true);
    // In reality, this would prompt for WebAuthn/Re-auth before saving to the DB/Core
    setTimeout(() => {
      setLoading(false);
      alert("Risk limits updated securely!");
    }, 1000);
  };

  return (
    <DashboardLayout>
      <div className="space-y-6 max-w-4xl">
        <div className="flex items-center gap-3">
          <ShieldAlert className="h-8 w-8 text-red-500" />
          <h1 className="text-3xl font-bold tracking-tight">Risk Limits</h1>
        </div>
        <p className="text-muted-foreground">
          Control the automated safety thresholds. The RiskManager will immediately enforce these values across all active orders.
        </p>

        <Card>
          <CardHeader>
            <CardTitle>Max Position Fraction</CardTitle>
            <CardDescription>
              The maximum percentage of the total portfolio balance that can be allocated to a single trade.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label>Value: {maxPosition[0]}%</Label>
            </div>
            <Slider 
              value={maxPosition} 
              onValueChange={(val) => setMaxPosition(val as number[])} 
              max={50} 
              step={1} 
            />
          </CardContent>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>Daily Loss Limit</CardTitle>
            <CardDescription>
              If the daily trailing PnL hits this percentage, all trading halts for 24 hours.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label>Value: -{dailyLoss[0]}%</Label>
            </div>
            <Slider 
              value={dailyLoss} 
              onValueChange={(val) => setDailyLoss(val as number[])} 
              max={100} 
              step={1} 
            />
          </CardContent>
        </Card>

        <Card className="border-red-500/50">
          <CardHeader>
            <CardTitle className="text-red-500">Total Drawdown Kill Switch</CardTitle>
            <CardDescription>
              If total portfolio value drops below this percentage of the initial balance, the bot liquidates all positions and halts indefinitely.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label className="text-red-500">Value: -{drawdownKill[0]}%</Label>
            </div>
            <Slider 
              value={drawdownKill} 
              onValueChange={(val) => setDrawdownKill(val as number[])} 
              max={100} 
              step={1} 
              className="[&_[role=slider]]:bg-red-500"
            />
          </CardContent>
          <CardFooter className="bg-red-500/10 border-t border-red-500/20 py-3">
            <p className="text-xs text-red-500 font-medium">
              Warning: Modifying this value requires a fresh security challenge.
            </p>
          </CardFooter>
        </Card>

        <div className="flex justify-end pt-4">
          <Button onClick={handleSave} disabled={loading} size="lg">
            {loading ? "Verifying & Saving..." : "Save Changes"}
          </Button>
        </div>
      </div>
    </DashboardLayout>
  );
}
