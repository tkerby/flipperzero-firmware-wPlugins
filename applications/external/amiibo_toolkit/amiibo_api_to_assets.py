"""
Read the JSON database from AmiiboAPI repository, process it, and convert it
into asset files for the Flipper Zero Amiibo Toolkit application.

The original data is re-assembled to be flat, so that Flipper Zero does not need
to look up multiple files to get all the information about a single Amiibo.
"""

import requests


AMIIBO_JSON_URL = "https://raw.githubusercontent.com/N3evin/AmiiboAPI/refs/heads/master/database/amiibo.json"
GAMES_INFO_JSON_URL = "https://raw.githubusercontent.com/N3evin/AmiiboAPI/refs/heads/master/database/games_info.json"


def fetch_json(url: str) -> dict:
    """
    Fetch JSON data from a given URL, and output it as a dictionary.
    :param url: The URL to fetch JSON data from.
    :type url: str
    :return: The JSON data as a dictionary.
    :rtype: dict
    """
    response = requests.get(url)

    response.raise_for_status()

    return response.json()


def process_amiibo_data(amiibo_data: dict):
    """
    Process the Amiibo data and convert it into asset files.
    :param amiibo_data: The Amiibo data as a dictionary.
    :type amiibo_data: dict
    """
    amiibos = amiibo_data.get("amiibos", {})
    amiibo_series = amiibo_data.get("amiibo_series", {})
    game_series = amiibo_data.get("game_series", {})
    types = amiibo_data.get("types", {})
    characters = amiibo_data.get("characters", {})

    amiibo_strs = {}
    amiibo_mapping_strs = {}

    for amiibo_id, amiibo in amiibos.items():
        amiibo_id_clean = amiibo_id[2:]  # Remove "0x" prefix
        name = amiibo.get("name", "Unknown")
        character = characters.get(f"0x{amiibo_id_clean[0:4]}", "Unknown")
        amiibo_series_name = amiibo_series.get(f"0x{amiibo_id_clean[12:14]}", "Unknown")
        game_series_name = game_series.get(f"0x{amiibo_id_clean[0:3]}", "Unknown")
        type_name = types.get(f"0x{amiibo_id_clean[6:8]}", "Unknown")

        release_info = amiibo.get("release", {})
        release_info_strs = []
        for region, date in release_info.items():
            release_info_strs.append(f"{region}.{date}")
        release_info_line = ",".join(release_info_strs)

        amiibo_strs[amiibo_id_clean] = (
            f"{name}|{character}|{amiibo_series_name}|{game_series_name}|{type_name}|{release_info_line}"
        )
        amiibo_mapping_strs[name] = amiibo_id_clean

    amiibo_strs = dict(sorted(amiibo_strs.items()))
    amiibo_mapping_strs = dict(sorted(amiibo_mapping_strs.items()))

    with open("files/amiibo.dat", "w") as amiibo_file:
        amiibo_file.write("Filetype: AmiTool Amiibo DB\n")
        amiibo_file.write("Version: 1\n")
        amiibo_file.write(f"AmiiboCount: {len(amiibos)}\n")
        amiibo_file.write("\n")

        for amiibo_id, amiibo_str in amiibo_strs.items():
            amiibo_file.write(f"{amiibo_id}: {amiibo_str}\n")

    with open("files/amiibo_name.dat", "w") as amiibo_name_file:
        amiibo_name_file.write("Filetype: AmiTool Amiibo Name DB\n")
        amiibo_name_file.write("Version: 1\n")
        amiibo_name_file.write(f"AmiiboCount: {len(amiibos)}\n")
        amiibo_name_file.write("\n")

        for amiibo_name, amiibo_id in amiibo_mapping_strs.items():
            amiibo_name_file.write(f"{amiibo_name}: {amiibo_id}\n")


def _generate_usage_string(info: dict, platform: str) -> str:
    """
    Generate a usage string for a given platform from the provided info.
    :param info: The usage info as a dictionary.
    :type info: dict
    :param platform: The platform name.
    :type platform: str
    :return: The generated usage string.
    :rtype: str
    """
    usage_str = f"{platform}^{info['gameName']}"

    if "amiiboUsage" in info:
        for usage in info["amiiboUsage"]:
            usage_str += f"^{usage['Usage']}*{usage.get('write', 'False')}"

    return usage_str


def process_games_info_data(games_info_data: dict):
    """
    Process the games info data and convert it into asset files.
    :param games_info_data: The games info data as a dictionary.
    :type games_info_data: dict
    """
    games_3ds = {}
    games_wii_u = {}
    games_switch = {}
    games_switch2 = {}

    with open("files/amiibo_usage.dat", "w") as usage_file:
        # Write headers
        usage_file.write("Filetype: AmiTool Usage DB\n")
        usage_file.write("Version: 1\n")
        usage_file.write(f"AmiiboCount: {len(games_info_data.get('amiibos', {}))}\n")
        usage_file.write("\n")

        # Write each Amiibo usage entry
        for amiibo_id, game_info in games_info_data.get("amiibos", {}).items():
            amiibo_id_clean = amiibo_id[2:]  # Remove "0x" prefix
            usage_strs = []

            for info_3ds in game_info.get("games3DS", []):
                if info_3ds["gameName"] not in games_3ds:
                    games_3ds[info_3ds["gameName"]] = [
                        amiibo_id_clean,
                    ]
                else:
                    games_3ds[info_3ds["gameName"]].append(amiibo_id_clean)

                usage_strs.append(_generate_usage_string(info_3ds, "3DS"))

            for info_wii_u in game_info.get("gamesWiiU", []):
                if info_wii_u["gameName"] not in games_wii_u:
                    games_wii_u[info_wii_u["gameName"]] = [
                        amiibo_id_clean,
                    ]
                else:
                    games_wii_u[info_wii_u["gameName"]].append(amiibo_id_clean)

                usage_strs.append(_generate_usage_string(info_wii_u, "WiiU"))

            for info_switch in game_info.get("gamesSwitch", []):
                if info_switch["gameName"] not in games_switch:
                    games_switch[info_switch["gameName"]] = [
                        amiibo_id_clean,
                    ]
                else:
                    games_switch[info_switch["gameName"]].append(amiibo_id_clean)

                usage_strs.append(_generate_usage_string(info_switch, "Switch"))

            for info_switch2 in game_info.get("gamesSwitch2", []):
                if info_switch2["gameName"] not in games_switch2:
                    games_switch2[info_switch2["gameName"]] = [
                        amiibo_id_clean,
                    ]
                else:
                    games_switch2[info_switch2["gameName"]].append(amiibo_id_clean)

                usage_strs.append(_generate_usage_string(info_switch2, "Switch2"))

            usage_file.write(f"{amiibo_id_clean}: {'|'.join(usage_strs)}\n")

    with open("files/game_3ds.dat", "w") as games_3ds_file:
        # Write headers
        games_3ds_file.write("Filetype: AmiTool Games DB\n")
        games_3ds_file.write("Version: 1\n")
        games_3ds_file.write("Console: 3DS\n")
        games_3ds_file.write(f"GameCount: {len(games_3ds)}\n")
        games_3ds_file.write("\n")

        # Write each game entry
        for game_name in games_3ds.keys():
            games_3ds_file.write(f"{game_name}\n")

    with open("files/game_3ds_mapping.dat", "w") as games_3ds_file:
        # Write headers
        games_3ds_file.write("Filetype: AmiTool Games Mapping DB\n")
        games_3ds_file.write("Version: 1\n")
        games_3ds_file.write("Console: 3DS\n")
        games_3ds_file.write(f"GameCount: {len(games_3ds)}\n")
        games_3ds_file.write("\n")

        # Write each game entry
        for game_name, amiibo_ids in games_3ds.items():
            games_3ds_file.write(f"{game_name}: {'|'.join(amiibo_ids)}\n")

    with open("files/game_wii_u.dat", "w") as games_wii_u_file:
        # Write headers
        games_wii_u_file.write("Filetype: AmiTool Games DB\n")
        games_wii_u_file.write("Version: 1\n")
        games_wii_u_file.write("Console: WiiU\n")
        games_wii_u_file.write(f"GameCount: {len(games_wii_u)}\n")
        games_wii_u_file.write("\n")

        # Write each game entry
        for game_name in games_wii_u.keys():
            games_wii_u_file.write(f"{game_name}\n")

    with open("files/game_wii_u_mapping.dat", "w") as games_wii_u_file:
        # Write headers
        games_wii_u_file.write("Filetype: AmiTool Games Mapping DB\n")
        games_wii_u_file.write("Version: 1\n")
        games_wii_u_file.write("Console: WiiU\n")
        games_wii_u_file.write(f"GameCount: {len(games_wii_u)}\n")
        games_wii_u_file.write("\n")

        # Write each game entry
        for game_name, amiibo_ids in games_wii_u.items():
            games_wii_u_file.write(f"{game_name}: {'|'.join(amiibo_ids)}\n")

    with open("files/game_switch.dat", "w") as games_switch_file:
        # Write headers
        games_switch_file.write("Filetype: AmiTool Games DB\n")
        games_switch_file.write("Version: 1\n")
        games_switch_file.write("Console: Switch\n")
        games_switch_file.write(f"GameCount: {len(games_switch)}\n")
        games_switch_file.write("\n")

        # Write each game entry
        for game_name in games_switch.keys():
            games_switch_file.write(f"{game_name}\n")

    with open("files/game_switch_mapping.dat", "w") as games_switch_file:
        # Write headers
        games_switch_file.write("Filetype: AmiTool Games Mapping DB\n")
        games_switch_file.write("Version: 1\n")
        games_switch_file.write("Console: Switch\n")
        games_switch_file.write(f"GameCount: {len(games_switch)}\n")
        games_switch_file.write("\n")

        # Write each game entry
        for game_name, amiibo_ids in games_switch.items():
            games_switch_file.write(f"{game_name}: {'|'.join(amiibo_ids)}\n")

    with open("files/game_switch2.dat", "w") as games_switch2_file:
        # Write headers
        games_switch2_file.write("Filetype: AmiTool Games DB\n")
        games_switch2_file.write("Version: 1\n")
        games_switch2_file.write("Console: Switch2\n")
        games_switch2_file.write(f"GameCount: {len(games_switch2)}\n")
        games_switch2_file.write("\n")

        # Write each game entry
        for game_name in games_switch2.keys():
            games_switch2_file.write(f"{game_name}\n")

    with open("files/game_switch2_mapping.dat", "w") as games_switch2_file:
        # Write headers
        games_switch2_file.write("Filetype: AmiTool Games Mapping DB\n")
        games_switch2_file.write("Version: 1\n")
        games_switch2_file.write("Console: Switch2\n")
        games_switch2_file.write(f"GameCount: {len(games_switch2)}\n")
        games_switch2_file.write("\n")

        # Write each game entry
        for game_name, amiibo_ids in games_switch2.items():
            games_switch2_file.write(f"{game_name}: {'|'.join(amiibo_ids)}\n")


def main():
    """
    Main function to fetch and process Amiibo data.
    """
    amiibo_data = fetch_json(AMIIBO_JSON_URL)
    games_info_data = fetch_json(GAMES_INFO_JSON_URL)

    process_amiibo_data(amiibo_data)
    process_games_info_data(games_info_data)


if __name__ == "__main__":
    main()
