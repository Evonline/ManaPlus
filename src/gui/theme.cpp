/*
 *  Gui Skinning
 *  Copyright (C) 2008  The Legend of Mazzeroth Development Team
 *  Copyright (C) 2009  Aethyra Development Team
 *  Copyright (C) 2009  The Mana World Development Team
 *  Copyright (C) 2009-2010  The Mana Developers
 *  Copyright (C) 2011  The ManaPlus Developers
 *
 *  This file is part of The ManaPlus Client.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/theme.h"

#include "client.h"
#include "configuration.h"
#include "logger.h"

#include "resources/dye.h"
#include "resources/image.h"
#include "resources/imageset.h"
#include "resources/resourcemanager.h"

#include "utils/dtor.h"
#include "utils/stringutils.h"
#include "utils/xml.h"

#include <physfs.h>

#include <algorithm>
#include <physfs.h>

#include "debug.h"

static std::string defaultThemePath;

std::string Theme::mThemePath;
std::string Theme::mThemeName;
Theme *Theme::mInstance = 0;

// Set the theme path...
static void initDefaultThemePath()
{
    ResourceManager *resman = ResourceManager::getInstance();
    defaultThemePath = branding.getStringValue("guiThemePath");

    logger->log("defaultThemePath: " + defaultThemePath);
    if (!defaultThemePath.empty() && resman->isDirectory(defaultThemePath))
        return;
    else
        defaultThemePath = "themes/";
}

Skin::Skin(ImageRect skin, Image *close, Image *stickyUp, Image *stickyDown,
           const std::string &filePath,
           const std::string &name):
    instances(0),
    mFilePath(filePath),
    mName(name),
    mBorder(skin),
    mCloseImage(close),
    mStickyImageUp(stickyUp),
    mStickyImageDown(stickyDown)
{}

Skin::~Skin()
{
    // Clean up static resources
    for (int i = 0; i < 9; i++)
    {
        delete mBorder.grid[i];
        mBorder.grid[i] = 0;
    }

    if (mCloseImage)
    {
        mCloseImage->decRef();
        mCloseImage = 0;
    }
    delete mStickyImageUp;
    mStickyImageUp = 0;
    delete mStickyImageDown;
    mStickyImageDown = 0;
}

void Skin::updateAlpha(float minimumOpacityAllowed)
{
    const float alpha = static_cast<float>(
        std::max(static_cast<double>(minimumOpacityAllowed),
        static_cast<double>(Client::getGuiAlpha())));

    for (int i = 0; i < 9; i++)
    {
        if (mBorder.grid[i])
            mBorder.grid[i]->setAlpha(alpha);
    }

    if (mCloseImage)
        mCloseImage->setAlpha(alpha);
    if (mStickyImageUp)
        mStickyImageUp->setAlpha(alpha);
    if (mStickyImageDown)
        mStickyImageDown->setAlpha(alpha);
}

int Skin::getMinWidth() const
{
    if (!mBorder.grid[ImageRect::UPPER_LEFT]
        || !mBorder.grid[ImageRect::UPPER_RIGHT])
    {
        return 1;
    }

    return mBorder.grid[ImageRect::UPPER_LEFT]->getWidth() +
           mBorder.grid[ImageRect::UPPER_RIGHT]->getWidth();
}

int Skin::getMinHeight() const
{
    if (!mBorder.grid[ImageRect::UPPER_LEFT]
        || !mBorder.grid[ImageRect::LOWER_LEFT])
    {
        return 1;
    }

    return mBorder.grid[ImageRect::UPPER_LEFT]->getHeight() +
           mBorder.grid[ImageRect::LOWER_LEFT]->getHeight();
}

Theme::Theme():
    Palette(THEME_COLORS_END),
    mMinimumOpacity(-1.0f),
    mProgressColors(ProgressColors(THEME_PROG_END))
{
    initDefaultThemePath();

    config.addListener("guialpha", this);
    loadColors();

    mColors[HIGHLIGHT].ch = 'H';
    mColors[CHAT].ch = 'C';
    mColors[GM].ch = 'G';
    mColors[PLAYER].ch = 'Y';
    mColors[WHISPER].ch = 'W';
    mColors[WHISPER_OFFLINE].ch = 'w';
    mColors[IS].ch = 'I';
    mColors[PARTY_CHAT_TAB].ch = 'P';
    mColors[GUILD_CHAT_TAB].ch = 'U';
    mColors[SERVER].ch = 'S';
    mColors[LOGGER].ch = 'L';
    mColors[HYPERLINK].ch = '<';
}

Theme::~Theme()
{
    delete_all(mSkins);
    config.removeListener("guialpha", this);
    delete_all(mProgressColors);
}

Theme *Theme::instance()
{
    if (!mInstance)
        mInstance = new Theme;

    return mInstance;
}

void Theme::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

gcn::Color Theme::getProgressColor(int type, float progress)
{
    int color[3] = {0, 0, 0};

    if (mInstance)
    {
        DyePalette *dye = mInstance->mProgressColors[type];

        if (dye)
            dye->getColor(progress, color);
        else
            logger->log("color not found: " + toString(type));
    }

    return gcn::Color(color[0], color[1], color[2]);
}

Skin *Theme::load(const std::string &filename, const std::string &defaultPath)
{
    // Check if this skin was already loaded

    SkinIterator skinIterator = mSkins.find(filename);
    if (mSkins.end() != skinIterator)
    {
        if (skinIterator->second)
            skinIterator->second->instances++;
        return skinIterator->second;
    }

    Skin *skin = readSkin(filename);

    if (!skin)
    {
        // Try falling back on the defaultPath if this makes sense
        if (filename != defaultPath)
        {
            logger->log("Error loading skin '%s', falling back on default.",
                        filename.c_str());

            skin = readSkin(defaultPath);
        }

        if (!skin)
        {
            logger->log(strprintf("Error: Loading default skin '%s' failed. "
                                  "Make sure the skin file is valid.",
                                  defaultPath.c_str()));
        }
    }

    // Add the skin to the loaded skins
    mSkins[filename] = skin;

    return skin;
}

void Theme::setMinimumOpacity(float minimumOpacity)
{
    if (minimumOpacity > 1.0f)
        return;

    mMinimumOpacity = minimumOpacity;
    updateAlpha();
}

void Theme::updateAlpha()
{
    for (SkinIterator iter = mSkins.begin(); iter != mSkins.end(); ++iter)
    {
        if (iter->second)
            iter->second->updateAlpha(mMinimumOpacity);
    }
}

void Theme::optionChanged(const std::string &)
{
    updateAlpha();
}

Skin *Theme::readSkin(const std::string &filename)
{
    if (filename.empty())
        return 0;

//    std::string filename = filename0;
//    ResourceManager *resman = ResourceManager::getInstance();
    logger->log("Loading skin '%s'.", filename.c_str());
//    filename = resman->mapPathToSkin(filename0);

    XML::Document doc(resolveThemePath(filename));
    xmlNodePtr rootNode = doc.rootNode();

    if (!rootNode || !xmlStrEqual(rootNode->name, BAD_CAST "skinset"))
        return 0;

    const std::string skinSetImage = XML::getProperty(rootNode, "image", "");

    if (skinSetImage.empty())
    {
        logger->log1("Theme::readSkin(): Skinset does not define an image!");
        return 0;
    }

    logger->log("Theme::load(): <skinset> defines '%s' as a skin image.",
                skinSetImage.c_str());

    Image *dBorders = Theme::getImageFromTheme(skinSetImage);
    ImageRect border;
    memset(&border, 0, sizeof(ImageRect));

    // iterate <widget>'s
    for_each_xml_child_node(widgetNode, rootNode)
    {
        if (!xmlStrEqual(widgetNode->name, BAD_CAST "widget"))
            continue;

        const std::string widgetType =
                XML::getProperty(widgetNode, "type", "unknown");
        if (widgetType == "Window")
        {
            // Iterate through <part>'s
            // LEEOR / TODO:
            // We need to make provisions to load in a CloseButton image. For
            // now it can just be hard-coded.
            for_each_xml_child_node(partNode, widgetNode)
            {
                if (!xmlStrEqual(partNode->name, BAD_CAST "part"))
                    continue;

                const std::string partType =
                        XML::getProperty(partNode, "type", "unknown");
                // TOP ROW
                const int xPos = XML::getProperty(partNode, "xpos", 0);
                const int yPos = XML::getProperty(partNode, "ypos", 0);
                const int width = XML::getProperty(partNode, "width", 1);
                const int height = XML::getProperty(partNode, "height", 1);

                if (partType == "top-left-corner")
                {
                    if (dBorders)
                    {
                        border.grid[0] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[0] = 0;
                    }
                }
                else if (partType == "top-edge")
                {
                    if (dBorders)
                    {
                        border.grid[1] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[1] = 0;
                    }
                }
                else if (partType == "top-right-corner")
                {
                    if (dBorders)
                    {
                        border.grid[2] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[2] = 0;
                    }
                }

                // MIDDLE ROW
                else if (partType == "left-edge")
                {
                    if (dBorders)
                    {
                        border.grid[3] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[3] = 0;
                    }
                }
                else if (partType == "bg-quad")
                {
                    if (dBorders)
                    {
                        border.grid[4] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[4] = 0;
                    }
                }
                else if (partType == "right-edge")
                {
                    if (dBorders)
                    {
                        border.grid[5] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[5] = 0;
                    }
                }

                // BOTTOM ROW
                else if (partType == "bottom-left-corner")
                {
                    if (dBorders)
                    {
                        border.grid[6] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[6] = 0;
                    }
                }
                else if (partType == "bottom-edge")
                {
                    if (dBorders)
                    {
                        border.grid[7] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[7] = 0;
                    }
                }
                else if (partType == "bottom-right-corner")
                {
                    if (dBorders)
                    {
                        border.grid[8] = dBorders->getSubImage(
                            xPos, yPos, width, height);
                    }
                    else
                    {
                        border.grid[8] = 0;
                    }
                }

                else
                {
                    logger->log("Theme::readSkin(): Unknown part type '%s'",
                                partType.c_str());
                }
            }
        }
        else
        {
            logger->log("Theme::readSkin(): Unknown widget type '%s'",
                        widgetType.c_str());
        }
    }

    if (dBorders)
        dBorders->decRef();

    logger->log1("Finished loading skin.");

    // Hard-coded for now until we update the above code
    // to look for window buttons
    Image *closeImage = Theme::getImageFromTheme("close_button.png");
    Image *sticky = Theme::getImageFromTheme("sticky_button.png");
    Image *stickyImageUp = 0;
    Image *stickyImageDown = 0;
    if (sticky)
    {
        stickyImageUp = sticky->getSubImage(0, 0, 15, 15);
        stickyImageDown = sticky->getSubImage(15, 0, 15, 15);
        sticky->decRef();
    }

    Skin *skin = new Skin(border, closeImage, stickyImageUp, stickyImageDown,
                          filename);
    skin->updateAlpha(mMinimumOpacity);
    return skin;
}

bool Theme::tryThemePath(std::string themeName)
{
    if (!themeName.empty())
    {
        std::string path = defaultThemePath + themeName;
        if (PHYSFS_exists(path.c_str()))
        {
            mThemePath = path;
            mThemeName = themeName;
            return true;
        }
    }

    return false;
}

void Theme::fillSkinsList(std::vector<std::string> &list)
{
    char **skins = PHYSFS_enumerateFiles(
        branding.getStringValue("guiThemePath").c_str());

    for (char **i = skins; *i != 0; i++)
    {
        if (PHYSFS_isDirectory((
            branding.getStringValue("guiThemePath") + *i).c_str()))
        {
            list.push_back(*i);
        }
    }

    PHYSFS_freeList(skins);
}

void Theme::fillFontsList(std::vector<std::string> &list)
{
    PHYSFS_permitSymbolicLinks(1);
    char **fonts = PHYSFS_enumerateFiles(
        branding.getStringValue("fontsPath").c_str());

    for (char **i = fonts; *i != 0; i++)
    {
        if (!PHYSFS_isDirectory((
            branding.getStringValue("fontsPath") + *i).c_str()))
        {
            list.push_back(*i);
        }
    }

    PHYSFS_freeList(fonts);
    PHYSFS_permitSymbolicLinks(0);
}

void Theme::selectSkin()
{
    prepareThemePath();
}

void Theme::prepareThemePath()
{
    initDefaultThemePath();

    mThemePath = "";
    mThemeName = "";

    // Try theme from settings
    if (tryThemePath(config.getValue("selectedSkin", "")))
        return;

    // Try theme from settings
    if (tryThemePath(config.getValue("theme", "")))
        return;

    // Try theme from branding
    if (tryThemePath(branding.getValue("theme", "")))
        return;

    if (mThemePath.empty())
        mThemePath = "graphics/gui";

    instance()->loadColors(mThemePath);

    logger->log("Selected Theme: " + mThemePath);
}

std::string Theme::resolveThemePath(const std::string &path)
{
    // Need to strip off any dye info for the existence tests
    int pos = static_cast<int>(path.find('|'));
    std::string file;
    if (pos > 0)
        file = path.substr(0, pos);
    else
        file = path;

    // Might be a valid path already
    if (PHYSFS_exists(file.c_str()))
        return path;

    // Try the theme
    file = getThemePath() + "/" + file;
    if (PHYSFS_exists(file.c_str()))
        return getThemePath() + "/" + path;

    // Backup
    return branding.getStringValue("guiPath") + path;
}

Image *Theme::getImageFromTheme(const std::string &path)
{
    ResourceManager *resman = ResourceManager::getInstance();
    return resman->getImage(resolveThemePath(path));
}

ImageSet *Theme::getImageSetFromTheme(const std::string &path,
                                      int w, int h)
{
    ResourceManager *resman = ResourceManager::getInstance();
    return resman->getImageSet(resolveThemePath(path), w, h);
}

static int readColorType(const std::string &type)
{
    static std::string colors[] =
    {
        "TEXT",
        "SHADOW",
        "OUTLINE",
        "BORDER",
        "PROGRESS_BAR",
        "BUTTON",
        "BUTTON_DISABLED",
        "TAB",
        "PARTY_CHAT_TAB",
        "PARTY_SOCIAL_TAB",
        "GUILD_CHAT_TAB",
        "GUILD_SOCIAL_TAB",
        "BACKGROUND",
        "BACKGROUND_GRAY",
        "SCROLLBAR_GRAY",
        "DROPDOWN_SHADOW",
        "HIGHLIGHT",
        "TAB_FLASH",
        "TAB_PLAYER_FLASH",
        "SHOP_WARNING",
        "ITEM_EQUIPPED",
        "ITEM_NOT_EQUIPPED",
        "CHAT",
        "GM",
        "PLAYER",
        "WHISPER",
        "WHISPER_OFFLINE",
        "IS",
        "SERVER",
        "LOGGER",
        "HYPERLINK",
        "UNKNOWN_ITEM",
        "GENERIC",
        "HEAD",
        "USABLE",
        "TORSO",
        "ONEHAND",
        "LEGS",
        "FEET",
        "TWOHAND",
        "SHIELD",
        "RING",
        "NECKLACE",
        "ARMS",
        "AMMO",
        "SERVER_VERSION_NOT_SUPPORTED",
        "WARNING",
        "CHARM",
        "PLAYER_ADVANCED"
    };

    if (type.empty())
        return -1;

    for (int i = 0; i < Theme::THEME_COLORS_END; i++)
    {
        if (compareStrI(type, colors[i]) == 0)
            return i;
    }

    return -1;
}

static gcn::Color readColor(const std::string &description)
{
    int size = static_cast<int>(description.length());
    if (size < 7 || description[0] != '#')
    {
        logger->log("Error, invalid theme color palette: %s",
                    description.c_str());
        return Palette::BLACK;
    }

    int v = 0;
    for (int i = 1; i < 7; ++i)
    {
        char c = description[i];
        int n;

        if ('0' <= c && c <= '9')
        {
            n = c - '0';
        }
        else if ('A' <= c && c <= 'F')
        {
            n = c - 'A' + 10;
        }
        else if ('a' <= c && c <= 'f')
        {
            n = c - 'a' + 10;
        }
        else
        {
            logger->log("Error, invalid theme color palette: %s",
                        description.c_str());
            return Palette::BLACK;
        }

        v = (v << 4) | n;
    }

    return gcn::Color(v);
}

static Palette::GradientType readColorGradient(const std::string &grad)
{
    static std::string grads[] =
    {
        "STATIC",
        "PULSE",
        "SPECTRUM",
        "RAINBOW"
    };

    if (grad.empty())
        return Palette::STATIC;

    for (int i = 0; i < 4; i++)
    {
        if (compareStrI(grad, grads[i]))
            return static_cast<Palette::GradientType>(i);
    }

    return Palette::STATIC;
}

static int readProgressType(const std::string &type)
{
    static std::string colors[] =
    {
        "DEFAULT",
        "HP",
        "MP",
        "NO_MP",
        "EXP",
        "INVY_SLOTS",
        "WEIGHT",
        "JOB"
    };

    if (type.empty())
        return -1;

    for (int i = 0; i < Theme::THEME_PROG_END; i++)
    {
        if (compareStrI(type, colors[i]) == 0)
            return i;
    }

    return -1;
}

void Theme::loadColors(std::string file)
{
//    if (file == mThemePath)
//        return; // No need to reload

    if (file == "")
        file = "colors.xml";
    else
        file += "/colors.xml";

    XML::Document doc(resolveThemePath(file));
    xmlNodePtr root = doc.rootNode();

    if (!root || !xmlStrEqual(root->name, BAD_CAST "colors"))
    {
        logger->log("Error loading colors file: %s", file.c_str());
        return;
    }

    logger->log("Loading colors file: %s", file.c_str());

    int type;
    std::string temp;
    gcn::Color color;
    GradientType grad;

    for_each_xml_child_node(node, root)
    {
        if (xmlStrEqual(node->name, BAD_CAST "color"))
        {
            type = readColorType(XML::getProperty(node, "id", ""));
            if (type < 0) // invalid or no type given
                continue;

            temp = XML::getProperty(node, "color", "");
            if (temp.empty()) // no color set, so move on
                continue;

            color = readColor(temp);
            grad = readColorGradient(XML::getProperty(node, "effect", ""));

            mColors[type].set(type, color, grad, 10);
        }
        else if (xmlStrEqual(node->name, BAD_CAST "progressbar"))
        {
            type = readProgressType(XML::getProperty(node, "id", ""));
            if (type < 0) // invalid or no type given
                continue;

            mProgressColors[type] = new DyePalette(XML::getProperty(node,
                                                   "color", ""));
        }
    }
}
