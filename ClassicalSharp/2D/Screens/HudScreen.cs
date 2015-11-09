﻿using System;
using System.Drawing;
using ClassicalSharp.GraphicsAPI;
using OpenTK.Input;

namespace ClassicalSharp {
	
	public class HudScreen : Screen {
		
		public HudScreen( Game game ) : base( game ) {
		}
		
		ChatScreen chat;
		BlockHotbarWidget hotbar;
		PlayerListWidget playerList;
		Font playerFont;
		
		public override void Render( double delta ) {
			if( game.HideGui ) return;
			
			bool showMinimal = game.GetActiveScreen != this;
			if( chat.HandlesAllInput )
				chat.RenderBackground();
			graphicsApi.Texturing = true;
			chat.Render( delta );
			if( !showMinimal )
				RenderHotbar( delta );
			
			//graphicsApi.BeginVbBatch( VertexFormat.Pos3fTex2fCol4b );
			//graphicsApi.BindTexture( game.TerrainAtlas.TexId );
			//IsometricBlockDrawer.Draw( game, (byte)Block.Brick, 30, game.Width - 50, game.Height - 20 );
			
			if( playerList != null ) {
				playerList.Render( delta );
				// NOTE: Should usually be caught by KeyUp, but just in case.
				if( !game.IsKeyDown( KeyBinding.PlayerList ) ) {
					playerList.Dispose();
					playerList = null;
				}
			}
			
			graphicsApi.Texturing = false;
			if( playerList == null && !showMinimal )
				DrawCrosshairs();
		}
		
		public void RenderHotbar( double delta ) { hotbar.Render( delta ); }
		
		const int crosshairExtent = 15, crosshairWeight = 2;
		void DrawCrosshairs() {
			int curCol = 150 + (int)( 50 * Math.Abs( Math.Sin( game.accumulator ) ) );
			FastColour col = new FastColour( curCol, curCol, curCol );
			float centreX = game.Width / 2, centreY = game.Height / 2;
			graphicsApi.Draw2DQuad( centreX - crosshairExtent, centreY - crosshairWeight,
			                       crosshairExtent * 2, crosshairWeight * 2, col );
			graphicsApi.Draw2DQuad( centreX - crosshairWeight, centreY - crosshairExtent,
			                       crosshairWeight * 2, crosshairExtent * 2, col );
		}
		
		public override void Dispose() {
			playerFont.Dispose();
			chat.Dispose();
			hotbar.Dispose();
			if( playerList != null )
				playerList.Dispose();
		}
		
		public void GainFocus() {
			if( game.CursorVisible )
				game.CursorVisible = false;
			game.Camera.RegrabMouse();
		}
		
		public void LoseFocus() {
			if( playerList != null ) {
				playerList.Dispose();
			}
			if( !game.CursorVisible )
				game.CursorVisible = true;
		}
		
		public override void OnResize( int oldWidth, int oldHeight, int width, int height ) {
			chat.OnResize( oldWidth, oldHeight, width, height );
			hotbar.OnResize( oldWidth, oldHeight, width, height );
			if( playerList != null ) {
				int deltaX = CalcDelta( width, oldWidth, Anchor.Centre );
				playerList.MoveTo( playerList.X + deltaX, height / 4 );
			}
		}
		
		public override void Init() {
			playerFont = new Font( "Arial", 12 );
			chat = new ChatScreen( game );
			chat.Init();
			hotbar = new BlockHotbarWidget( game );
			hotbar.Init();
		}
		
		public override bool HandlesAllInput {
			get { return chat.HandlesAllInput; }
		}

		public override bool HandlesKeyPress( char key ) {
			return chat.HandlesKeyPress( key );
		}
		
		public override bool HandlesKeyDown( Key key ) {
			if( key == game.Mapping( KeyBinding.PlayerList ) ) {
				if( playerList == null ) {
					if( game.Network.UsingExtPlayerList ) {
						playerList = new ExtPlayerListWidget( game, playerFont );
					} else {
						playerList = new NormalPlayerListWidget( game, playerFont );
					}
					playerList.Init();
					playerList.MoveTo( playerList.X, game.Height / 4 );
				}
			}
			if( chat.HandlesKeyDown( key ) ) {
				return true;
			}
			return hotbar.HandlesKeyDown( key );
		}
		
		public override bool HandlesKeyUp( Key key ) {
			if( key == game.Mapping( KeyBinding.PlayerList ) ) {
				if( playerList != null ) {
					playerList.Dispose();
					playerList = null;
					return true;
				}
			}
			return false;
		}
		
		public void OpenTextInputBar( string text ) {
			chat.OpenTextInputBar( text );
		}
		
		public override bool HandlesMouseScroll( int delta ) {
			return chat.HandlesMouseScroll( delta );
		}
		
		public override bool HandlesMouseClick( int mouseX, int mouseY, MouseButton button ) {
			if( button != MouseButton.Left || playerList == null || !HandlesAllInput ) return false;
			
			string name = playerList.GetNameUnder( mouseX, mouseY );
			if( name == null ) return false;
			chat.AppendTextToInput( name + " " );
			return true;
		}
	}
}
