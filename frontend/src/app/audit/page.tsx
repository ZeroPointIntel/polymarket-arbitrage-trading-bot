"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { FileText } from "lucide-react";

export default function AuditPage() {
  return (
    <DashboardLayout>
      <div className="space-y-6 max-w-5xl">
        <div className="flex items-center gap-3">
          <FileText className="h-8 w-8 text-primary" />
          <h1 className="text-3xl font-bold tracking-tight">Audit Log</h1>
        </div>
        <p className="text-muted-foreground">
          Append-only cryptographic record of parameter changes and system events.
        </p>

        <Card>
          <CardHeader>
            <CardTitle>System Events</CardTitle>
            <CardDescription>
              A table of authenticated system modifications will appear here.
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="flex flex-col items-center justify-center p-12 border border-dashed rounded-lg text-muted-foreground bg-muted/20">
              <FileText className="h-12 w-12 mb-4 opacity-50" />
              <p>No audit events recorded yet.</p>
            </div>
          </CardContent>
        </Card>
      </div>
    </DashboardLayout>
  );
}
