import React from "react";
import { cn } from "@/lib/utils";

interface PageHeaderProps extends Omit<React.HTMLAttributes<HTMLDivElement>, "title"> {
  title: React.ReactNode;
  description?: React.ReactNode;
  icon?: React.ElementType;
}

export function PageHeader({ title, description, icon: Icon, className, ...props }: PageHeaderProps) {
  return (
    <div className={cn("flex flex-col gap-2 mb-6", className)} {...props}>
      <div className="flex items-center gap-3">
        {Icon && <Icon className="h-8 w-8 text-white" />}
        <h1 className="text-3xl font-bold tracking-tight text-white">
          {title}
        </h1>
      </div>
      {description && (
        <p className="text-muted-foreground max-w-2xl text-base">
          {description}
        </p>
      )}
    </div>
  );
}
