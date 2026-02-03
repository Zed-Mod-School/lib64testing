import random
import requests
from flask import Flask, jsonify

app = Flask(__name__)

PLAYER_USERNAMES = [
    "morisv", "blidan", "ye",
    "mitchflowerpower", "zfg1",
    "cheese", "niftski", "kosmicd12"
]



def fetch_user_id(username):
    resp = requests.get(f"https://www.speedrun.com/api/v1/users/{username}")
    if not resp.ok: return None
    return resp.json().get("data", {}).get("id")

def fetch_user_runs(user_id):
    resp = requests.get(f"https://www.speedrun.com/api/v1/runs?user={user_id}&status=verified&max=50")
    if not resp.ok: return []
    return [run["game"] for run in resp.json().get("data", []) if "game" in run]

def fetch_user_profile(username):
    resp = requests.get(f"https://www.speedrun.com/api/v1/users/{username}")
    if not resp.ok: return None
    data = resp.json().get("data", {})
    return {
        "name": data["names"]["international"],
        "avatar_url": data["assets"]["image"]["uri"]
    }

@app.route("/api/random_game_round")
def random_game_round():
    selected = random.sample(PLAYER_USERNAMES, 5)
    user_ids = [fetch_user_id(u) for u in selected]

    run_data = {}
    for username, uid in zip(selected, user_ids):
        if not uid: continue
        run_data[username] = fetch_user_runs(uid)

    # Tally game frequencies
    game_counts = {}
    for runs in run_data.values():
        for g in runs:
            game_counts[g] = game_counts.get(g, 0) + 1

    if not game_counts:
        return jsonify({"error": "No common games found"}), 400

    target_game = max(game_counts.items(), key=lambda x: x[1])[0]
    profiles = [fetch_user_profile(name) for name in selected]

    return jsonify({
        "profiles": profiles,
        "answer_game_id": target_game,  # (don't expose in frontend)
    })


if __name__ == "__main__":
    app.run(debug=True)
