import React from "react";
import { cn } from "@/lib/utils";

interface PageHeaderProps extends Omit<React.HTMLAttributes<HTMLDivElement>, "title"> {
  title: React.ReactNode;
  description?: React.ReactNode;
  icon?: React.ElementType;
}

export function PageHeader({ title, description, icon: Icon, className, ...props }: PageHeaderProps) {
  return (
    <div className={cn("flex flex-col gap-3 mb-8", className)} {...props}>
      <div className="flex items-center gap-4">
        {Icon && (
          <div className="flex items-center justify-center h-10 w-10 rounded-2xl bg-white/10 border border-white/10">
            <Icon className="h-5 w-5 text-white/90" />
          </div>
        )}
        <h1 className="text-4xl font-heading font-bold tracking-tight text-gradient">
          {title}
        </h1>
      </div>
      {description && (
        <p className="text-white/50 max-w-2xl text-[15px] leading-relaxed tracking-wide">
          {description}
        </p>
      )}
    </div>
  );
}
