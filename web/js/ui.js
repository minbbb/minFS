import { state, isImg, isVideo } from "./state.js";
import { esc, fmt, join, crumbs } from "./utils.js";

const list = document.getElementById("list");
const fsEl = document.getElementById("fs");
const crumbsEl = document.getElementById("crumbs");
const errEl = document.getElementById("err");

function thumbFor(it) {
    if (isImg(it.name)) {
        const u = "/img?path=" + encodeURIComponent(join(state.cur, it.name));
        return `<td class='thumb-cell'><img class='thumb' loading='lazy' src='${u}' data-path='${esc(join(state.cur, it.name))}' data-type='img' alt=''></td>`;
    }
    if (isVideo(it.name)) {
        return `<td class='thumb-cell'><span class='thumb vid-thumb' data-path='${esc(join(state.cur, it.name))}' data-type='video'><svg class='vid' viewBox='0 0 24 24' width='22' height='22' aria-hidden='true'><rect x='4' y='5' width='16' height='14' rx='2' fill='none' stroke='currentColor' stroke-width='1.5'/><path d='M10 9l6 3-6 3z' fill='currentColor'/></svg></span></td>`;
    }
    return `<td class='thumb-cell'></td>`;
}

function render(data) {
    list.innerHTML = "";
    errEl.style.display = "none";
    if (data.error) {
        crumbsEl.innerHTML = crumbs(data.path);
        errEl.innerHTML = "";
        errEl.appendChild(document.createTextNode("Error: " + data.error + " "));
        const go = document.createElement("a");
        go.href = "#";
        go.className = "gohome";
        go.textContent = "To root";
        errEl.appendChild(go);
        errEl.style.display = "block";
        return;
    }
    const hasThumbs = data.items.some(
        (it) => !it.dir && (isImg(it.name) || isVideo(it.name)),
    );
    fsEl.classList.toggle("no-thumbs", !hasThumbs);
    crumbsEl.innerHTML = crumbs(data.path);
    if (data.parent) {
        const tr = document.createElement("tr");
        tr.className = "dir";
        tr.setAttribute("data-p", data.parent);
        tr.innerHTML = `<td class='thumb-cell'></td><td><span class='ico'>&#8592;</span><span class='nm'>..</span></td><td class='size'></td><td class='act'></td>`;
        list.appendChild(tr);
    }
    data.items.forEach((it) => {
        const tr = document.createElement("tr");
        if (it.dir) {
            tr.className = "dir";
            tr.setAttribute("data-p", join(state.cur, it.name));
            tr.innerHTML = `<td class='thumb-cell'></td><td><span class='ico'>&#9656;</span><span class='nm'>${esc(it.name)}</span></td><td class='size'></td><td class='act'></td>`;
        } else {
            tr.innerHTML =
                thumbFor(it) +
                `<td><span class='ico'>&#183;</span><span class='nm'>${esc(it.name)}</span></td><td class='size'>${fmt(it.size)}</td><td class='act'><a class='dl' href='/download?path=${encodeURIComponent(join(state.cur, it.name))}' download>&#8595;</a></td>`;
        }
        list.appendChild(tr);
    });
}

export async function load(p) {
    state.cur = p;
    if (p === undefined) {
        state.cur = "/";
        try {
            const r = await fetch("/api");
            const data = await r.json();
            if (data.path) state.cur = data.path;
            render(data);
        } catch (e) {
            errEl.textContent = "Network error: " + e;
            errEl.style.display = "block";
        }
        return;
    }
    crumbsEl.innerHTML = crumbs(p);
    try {
        const r = await fetch("/api?path=" + encodeURIComponent(p));
        render(await r.json());
    } catch (e) {
        errEl.textContent = "Network error: " + e;
        errEl.style.display = "block";
    }
}