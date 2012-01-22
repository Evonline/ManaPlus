/*
 *  The ManaPlus Client
 *  Copyright (C) 2004-2009  The Mana World Development Team
 *  Copyright (C) 2009-2010  The Mana Developers
 *  Copyright (C) 2011-2012  The ManaPlus Developers
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

#ifndef MUSIC_H
#define MUSIC_H

#include "resources/resource.h"

#include <SDL_mixer.h>

/**
 * Defines a class for loading and storing music.
 */
class Music : public Resource
{
    public:
        /**
         * Destructor.
         */
        virtual ~Music();

        /**
         * Loads a music from a buffer in memory.
         *
         * @param rw         The SDL_RWops to load the music data from.
         *
         * @return <code>NULL</code> if the an error occurred, a valid pointer
         *         otherwise.
         */
        static Resource *load(SDL_RWops *rw);

        /**
         * Plays the music.
         *
         * @param loops     Number of times to repeat the playback.
         *
         * @return <code>true</code> if the playback started properly
         *         <code>false</code> otherwise.
         */
        virtual bool play(int loops);

        /**
         * Stops the music.
         */
        virtual void stop();

    protected:
        /**
         * Constructor.
         */
        Music(Mix_Chunk *music);

        //Mix_Music *music;
        Mix_Chunk *mChunk;
        int mChannel;
};

#endif
