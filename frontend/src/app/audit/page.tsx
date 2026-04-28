"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { PageContainer } from "@/components/shared/PageContainer";
import { PageHeader } from "@/components/shared/PageHeader";
import { GlassCard, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/shared/GlassCard";
import { FileText } from "lucide-react";

export default function AuditPage() {
  return (
    <DashboardLayout>
      <PageContainer>
        <PageHeader 
          title="Audit Log" 
          description="Append-only cryptographic record of parameter changes and system events."
          icon={FileText}
        />

        <GlassCard>
          <CardHeader>
            <CardTitle>System Events</CardTitle>
            <CardDescription>
              A table of authenticated system modifications will appear here.
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="flex flex-col items-center justify-center p-12 border border-white/5 border-dashed rounded-lg text-muted-foreground bg-black/20">
              <FileText className="h-12 w-12 mb-4 opacity-30" />
              <p>No audit events recorded yet.</p>
            </div>
          </CardContent>
        </GlassCard>
      </PageContainer>
    </DashboardLayout>
  );
}
