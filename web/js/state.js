export const state = {
    cur: "/",
    media: [],
    mediaIndex: 0,
};

export const IMG_EXT = new Set(["jpg", "jpeg", "png", "gif", "webp", "bmp", "svg"]);
export const VIDEO_EXT = new Set(["mp4", "mkv", "avi", "mov", "webm", "wmv", "m4v", "flv", "mpg", "mpeg", "ts", "3gp", "ogv", "m2ts", "vob"]);

export function ext(name) {
    const n = name.toLowerCase();
    const i = n.lastIndexOf(".");
    return i < 0 ? "" : n.substr(i + 1);
}
export const isImg = (name) => IMG_EXT.has(ext(name));
export const isVideo = (name) => VIDEO_EXT.has(ext(name));