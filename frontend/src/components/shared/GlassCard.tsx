import { Card, CardHeader, CardTitle, CardDescription, CardContent, CardFooter } from "@/components/ui/card";
import { cn } from "@/lib/utils";
import React from "react";

interface GlassCardProps extends React.HTMLAttributes<HTMLDivElement> {
  children: React.ReactNode;
}

export function GlassCard({ className, children, ...props }: GlassCardProps) {
  return (
    <Card 
      className={cn(
        "bg-[#0a0a0a] border border-white/10 shadow-[inset_0_1px_0_0_rgba(255,255,255,0.05)] transition-all hover:border-white/20",
        className
      )} 
      {...props}
    >
      {children}
    </Card>
  );
}

// Re-export the Shadcn components so we can import them from this file
export { CardHeader, CardTitle, CardDescription, CardContent, CardFooter };
