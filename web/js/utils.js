export function esc(s) {
    let out = "";
    for (let i = 0; i < s.length; i++) {
        const c = s[i];
        if (c === "&") out += "&amp;";
        else if (c === "<") out += "&lt;";
        else if (c === ">") out += "&gt;";
        else if (c === '"') out += "&quot;";
        else if (c === "'") out += "&#39;";
        else out += c;
    }
    return out;
}

export function fmt(n) {
    if (!isFinite(n) || n < 0) return "-";
    if (n < 1024) return n + " B";
    const units = ["KB", "MB", "GB", "TB"];
    let i = -1;
    do {
        n /= 1024;
        i++;
    } while (n >= 1024 && i < units.length - 1);
    return n.toFixed(1) + " " + units[i];
}

export const join = (p, name) => (p === "/" ? "/" + name : p + "/" + name);

export function crumbs(p) {
    const parts = p.split("/").filter(Boolean);
    let html = `<a class='crumb' href='#' data-p='/'>/</a>`;
    let acc = "";
    for (const part of parts) {
        acc += "/" + part;
        html += `<span class='sep'> / </span><a class='crumb' href='#' data-p='${esc(acc)}'>${esc(part)}</a>`;
    }
    return html;
}