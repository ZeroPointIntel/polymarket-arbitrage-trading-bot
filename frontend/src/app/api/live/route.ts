import { WebSocket } from 'ws';

export const dynamic = "force-dynamic";

export async function GET(req: Request) {
  const stream = new ReadableStream({
    start(controller) {
      const encoder = new TextEncoder();
      
      // Connect to the C++ Core's LiveStateServer
      const ws = new WebSocket('ws://127.0.0.1:8080');
      
      ws.on('open', () => {
        console.log('[SSE] Connected to C++ LiveStateServer');
      });

      ws.on('message', (data) => {
        // Data from C++ is a JSON string. Push it directly to SSE.
        const payload = `data: ${data.toString()}\n\n`;
        controller.enqueue(encoder.encode(payload));
      });
      
      ws.on('close', () => {
        console.log('[SSE] C++ LiveStateServer disconnected');
        try { controller.close(); } catch (e) {}
      });

      ws.on('error', (err) => {
        console.error('[SSE] WebSocket error:', err);
        try { controller.error(err); } catch (e) {}
      });

      req.signal.addEventListener("abort", () => {
        console.log('[SSE] Client disconnected, closing WS');
        ws.close();
      });
    },
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache, no-transform",
      "Connection": "keep-alive",
    },
  });
}
