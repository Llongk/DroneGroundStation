import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "DroneGroundStation 云端监控中心",
  description: "手机与 STM32 无人机遥测、航线、曲线和告警的实时云端监控页面",
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return (
    <html lang="zh-CN">
      <body>{children}</body>
    </html>
  );
}
