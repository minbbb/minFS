import { state } from "./state.js";

const viewerEl = document.getElementById("viewer");
const viewerImg = document.getElementById("viewer-img");
const viewerVideo = document.getElementById("viewer-video");
const viewerCounter = document.querySelector(".v-counter");
const vClose = document.querySelector(".v-close");
const vPrev = document.querySelector(".v-prev");
const vNext = document.querySelector(".v-next");

function stopVideo() {
    viewerVideo.pause();
    viewerVideo.removeAttribute("src");
    viewerVideo.load();
}

export function collectMedia() {
    state.media = [];
    document.querySelectorAll("[data-path][data-type]").forEach((t) => {
        state.media.push({
            path: t.getAttribute("data-path"),
            type: t.getAttribute("data-type"),
        });
    });
}

export function openViewer(idx) {
    state.mediaIndex = idx;
    const item = state.media[idx];
    if (item.type === "video") {
        viewerImg.hidden = true;
        viewerVideo.hidden = false;
        stopVideo();
        viewerVideo.src = "/video?path=" + encodeURIComponent(item.path);
    } else {
        viewerVideo.hidden = true;
        viewerImg.hidden = false;
        viewerImg.src = "/img?path=" + encodeURIComponent(item.path);
        stopVideo();
    }
    viewerCounter.textContent = idx + 1 + " / " + state.media.length;
    viewerEl.classList.add("open");
    document.body.style.overflow = "hidden";
}

export function closeViewer() {
    viewerEl.classList.remove("open");
    viewerImg.src = "";
    stopVideo();
    document.body.style.overflow = "";
}

const showPrev = () =>
    openViewer((state.mediaIndex - 1 + state.media.length) % state.media.length);
const showNext = () => openViewer((state.mediaIndex + 1) % state.media.length);

export function initViewer() {
    viewerEl.addEventListener("click", (e) => {
        if (e.target === viewerEl) closeViewer();
    });
    vClose.addEventListener("click", closeViewer);
    vPrev.addEventListener("click", (e) => {
        e.stopPropagation();
        showPrev();
    });
    vNext.addEventListener("click", (e) => {
        e.stopPropagation();
        showNext();
    });

    document.addEventListener("keydown", (e) => {
        if (!viewerEl.classList.contains("open")) return;
        if (e.key === "Escape") closeViewer();
        else if (e.key === "ArrowLeft") showPrev();
        else if (e.key === "ArrowRight") showNext();
    });

    let touchX = 0;
    viewerEl.addEventListener(
        "touchstart",
        (e) => {
            touchX = e.changedTouches[0].screenX;
        },
        { passive: true },
    );
    viewerEl.addEventListener(
        "touchend",
        (e) => {
            const dx = e.changedTouches[0].screenX - touchX;
            if (Math.abs(dx) > 50) {
                if (dx > 0) showPrev();
                else showNext();
            }
        },
        { passive: true },
    );
}