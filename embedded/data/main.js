document.addEventListener("DOMContentLoaded", () => {
    initMobileSidebar();
    loadConfiguration();
    initFormHandlers();
});

function initMobileSidebar() {
    const sidebar = document.getElementById("sidebar");
    const overlay = document.getElementById("sidebar-overlay");
    const toggleBtn = document.getElementById("sidebar-toggle-btn");
    const closeBtn = document.getElementById("sidebar-close-btn");

    function openSidebar() {
        sidebar.classList.add("open");
        overlay.classList.add("active");
        document.body.style.overflow = "hidden";
    }

    function closeSidebar() {
        sidebar.classList.remove("open");
        overlay.classList.remove("active");
        document.body.style.overflow = "";
    }

    if (toggleBtn) toggleBtn.addEventListener("click", openSidebar);
    if (closeBtn) closeBtn.addEventListener("click", closeSidebar);
    if (overlay) overlay.addEventListener("click", closeSidebar);
}

async function loadConfiguration() {
    try {
        const response = await fetch("/api/config");
        if (!response.ok) throw new Error(`HTTP error: ${response.status}`);
        const data = await response.json();

        if (data.deviceApPassword !== undefined) document.getElementById("deviceApPassword").value = data.deviceApPassword;
        if (data.wifiSsid !== undefined) document.getElementById("wifiSsid").value = data.wifiSsid;
        if (data.wifiPassword !== undefined) document.getElementById("wifiPassword").value = data.wifiPassword;
        if (data.wsHost !== undefined) document.getElementById("wsHost").value = data.wsHost;
        if (data.wsPort !== undefined) document.getElementById("wsPort").value = data.wsPort;
        if (data.authKey !== undefined) document.getElementById("authKey").value = data.authKey;

    } catch (error) {
        showToast("Không thể nạp dữ liệu cấu hình từ thiết bị", "error");
        console.error("Lỗi nạp config:", error);
    }
}

function initFormHandlers() {
    const form = document.getElementById("config-form");
    const restartBtn = document.getElementById("btn-restart");

    form.addEventListener("submit", async (e) => {
        e.preventDefault();

        const payload = {
            deviceApPassword: document.getElementById("deviceApPassword").value,
            wifiSsid: document.getElementById("wifiSsid").value,
            wifiPassword: document.getElementById("wifiPassword").value,
            wsHost: document.getElementById("wsHost").value,
            wsPort: parseInt(document.getElementById("wsPort").value, 10),
            authKey: document.getElementById("authKey").value
        };

        try {
            const response = await fetch("/api/config", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload)
            });

            if (response.ok) {
                showToast("Lưu cấu hình thành công!", "success");
            } else {
                showToast("Lỗi khi lưu cấu hình!", "error");
            }
        } catch (error) {
            showToast("Mất kết nối với thiết bị!", "error");
            console.error("Lỗi gửi config:", error);
        }
    });

    restartBtn.addEventListener("click", async () => {
        if (!confirm("Xác nhận khởi động lại thiết bị?")) return;

        try {
            const response = await fetch("/api/restart", { method: "POST" });
            if (response.ok) {
                showToast("Thiết bị đang khởi động lại...", "success");
            }
        } catch (error) {
            showToast("Không thể gửi lệnh khởi động lại", "error");
        }
    });
}

function showToast(message, type = "success") {
    const toast = document.getElementById("toast");
    const icon = type === "success" 
        ? `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="18" height="18" fill="currentColor">
            <path d="M256 512A256 256 0 1 0 256 0a256 256 0 1 0 0 512zM369 209L241 337c-9.4 9.4-24.6 9.4-33.9 0l-64-64c-9.4-9.4-9.4-24.6 0-33.9s24.6-9.4 33.9 0l47 47L335 175c9.4-9.4 24.6-9.4 33.9 0s9.4 24.6 0 33.9z"/>
           </svg>`
        : `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="18" height="18" fill="currentColor">
            <path d="M256 512A256 256 0 1 0 256 0a256 256 0 1 0 0 512zM216 336h80c13.3 0 24 10.7 24 24s-10.7 24-24 24H216c-13.3 0-24-10.7-24-24s10.7-24 24-24zm40-208c13.3 0 24 10.7 24 24v112c0 13.3-10.7 24-24 24s-24-10.7-24-24V152c0-13.3 10.7-24 24-24z"/>
           </svg>`;
    toast.innerHTML = `${icon} <span>${message}</span>`;
    toast.className = `toast ${type}`;

    setTimeout(() => {
        toast.className = "toast hidden";
    }, 4000);
}