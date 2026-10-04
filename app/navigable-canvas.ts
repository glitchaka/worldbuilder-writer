export type Camera = {
  scale: number;
  x: number;
  y: number;
};

export type CanvasPoint = {
  x: number;
  y: number;
};

export type CanvasBounds = {
  minX: number;
  minY: number;
  maxX: number;
  maxY: number;
};

export function clampScale(value: number, minimum: number, maximum: number) {
  return Math.max(minimum, Math.min(maximum, value));
}

export function zoomCameraAt(
  camera: Camera,
  nextScale: number,
  anchor: CanvasPoint,
  minimum: number,
  maximum: number,
): Camera {
  const scale = clampScale(nextScale, minimum, maximum);
  const worldX = (anchor.x - camera.x) / camera.scale;
  const worldY = (anchor.y - camera.y) / camera.scale;
  return {
    scale,
    x: anchor.x - worldX * scale,
    y: anchor.y - worldY * scale,
  };
}

export function screenToWorld(camera: Camera, point: CanvasPoint): CanvasPoint {
  return {
    x: (point.x - camera.x) / camera.scale,
    y: (point.y - camera.y) / camera.scale,
  };
}

export function visibleWorldBounds(camera: Camera, width: number, height: number, margin = 0): CanvasBounds {
  const topLeft = screenToWorld(camera, { x: -margin, y: -margin });
  const bottomRight = screenToWorld(camera, { x: width + margin, y: height + margin });
  return {
    minX: Math.min(topLeft.x, bottomRight.x),
    minY: Math.min(topLeft.y, bottomRight.y),
    maxX: Math.max(topLeft.x, bottomRight.x),
    maxY: Math.max(topLeft.y, bottomRight.y),
  };
}

export function fitCameraToBounds(
  bounds: CanvasBounds,
  viewportWidth: number,
  viewportHeight: number,
  minimum: number,
  maximum: number,
  padding = 56,
): Camera {
  const contentWidth = Math.max(1, bounds.maxX - bounds.minX);
  const contentHeight = Math.max(1, bounds.maxY - bounds.minY);
  const availableWidth = Math.max(1, viewportWidth - padding * 2);
  const availableHeight = Math.max(1, viewportHeight - padding * 2);
  const scale = clampScale(Math.min(availableWidth / contentWidth, availableHeight / contentHeight), minimum, maximum);
  const centerX = (bounds.minX + bounds.maxX) / 2;
  const centerY = (bounds.minY + bounds.maxY) / 2;
  return {
    scale,
    x: viewportWidth / 2 - centerX * scale,
    y: viewportHeight / 2 - centerY * scale,
  };
}

export function boundsOverlap(left: CanvasBounds, right: CanvasBounds) {
  return left.minX <= right.maxX && left.maxX >= right.minX && left.minY <= right.maxY && left.maxY >= right.minY;
}

export function pointBounds(point: CanvasPoint, radiusX: number, radiusY: number): CanvasBounds {
  return {
    minX: point.x - radiusX,
    minY: point.y - radiusY,
    maxX: point.x + radiusX,
    maxY: point.y + radiusY,
  };
}
