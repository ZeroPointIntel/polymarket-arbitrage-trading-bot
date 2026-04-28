"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { History } from "lucide-react";

export default function HistoryPage() {
  return (
    <DashboardLayout>
      <div className="space-y-6 max-w-5xl">
        <div className="flex items-center gap-3">
          <History className="h-8 w-8 text-primary" />
          <h1 className="text-3xl font-bold tracking-tight">Trade History</h1>
        </div>
        <p className="text-muted-foreground">
          Historical record of all executed trades, PnL, and fees.
        </p>

        <Card>
          <CardHeader>
            <CardTitle>Recent Trades</CardTitle>
            <CardDescription>
              A table of the latest closed positions will appear here once connected to the Database/Core.
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="flex flex-col items-center justify-center p-12 border border-dashed rounded-lg text-muted-foreground bg-muted/20">
              <History className="h-12 w-12 mb-4 opacity-50" />
              <p>No trades executed yet in this session.</p>
            </div>
          </CardContent>
        </Card>
      </div>
    </DashboardLayout>
  );
}
