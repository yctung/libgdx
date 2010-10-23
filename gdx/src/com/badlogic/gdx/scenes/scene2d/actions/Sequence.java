package com.badlogic.gdx.scenes.scene2d.actions;

import java.util.ArrayList;
import java.util.List;

import com.badlogic.gdx.scenes.scene2d.Action;
import com.badlogic.gdx.scenes.scene2d.Actor;
import com.badlogic.gdx.utils.Pool;
import com.badlogic.gdx.utils.Pool.PoolObjectFactory;

public class Sequence implements Action
{
	static final Pool<Sequence> pool = new Pool<Sequence>( new PoolObjectFactory<Sequence>() {
		@Override
		public Sequence createObject() 
		{
			return new Sequence( );
		}
	}, 100 );
	
	private final List<Action> actions = new ArrayList<Action>( );
	private Actor target;
	private int currAction = 0;
	
	public static Sequence $( Action ... actions )
	{
		Sequence action = pool.newObject();
		int len = actions.length;
		for( int i = 0; i < len; i++ )
			action.actions.add( actions[i] );
		action.currAction = 0;
		return action;
	}
	
	@Override
	public void setTarget(Actor actor) 
	{
		this.target = actor;
		if( actions.size() > 0 )
			actions.get(0).setTarget( target );
	}

	@Override
	public void act(float delta) 
	{
		if( actions.size() == 0 )
		{
			currAction = 1;
			return;
		}
		
		actions.get(currAction).act( delta );
		if( actions.get(currAction).isDone() )
		{
			currAction++;
			if( currAction < actions.size() )
				actions.get(currAction).setTarget( target );
		}
	}

	@Override
	public boolean isDone() 
	{
		boolean done = currAction == actions.size();
		if( done )
			pool.free( this );
		return done;
	}
}