import { state } from "./state.js";
import { load } from "./ui.js";
import { collectMedia, openViewer } from "./viewer.js";

export function initEvents() {
    document.addEventListener("click", (e) => {
        const go = e.target.closest(".gohome");
        if (go) {
            e.preventDefault();
            load("/");
            return;
        }
        const crumb = e.target.closest("a.crumb");
        if (crumb) {
            e.preventDefault();
            load(crumb.getAttribute("data-p"));
            return;
        }
        const row = e.target.closest("tr.dir");
        if (row && row.getAttribute("data-p")) {
            load(row.getAttribute("data-p"));
            return;
        }
        const thumb = e.target.closest("[data-path][data-type]");
        if (thumb) {
            collectMedia();
            const idx = state.media.findIndex(
                (m) =>
                    m.path === thumb.getAttribute("data-path") &&
                    m.type === thumb.getAttribute("data-type"),
            );
            openViewer(idx >= 0 ? idx : 0);
        }
    });
}