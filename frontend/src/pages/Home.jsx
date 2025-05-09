import React from 'react'
import { useNavigate } from 'react-router-dom'
import styles from '../styles/Home.module.css'

export default function Home() {
	const navigate = useNavigate()

	const playWithBot = () => {
		navigate('/game/bot')
	}

	const playWithFriend = () => {
		navigate('/game/player')
	}

	return (
		<div className={styles.wrapper}>
			<h1 className={styles.title}>Monke Chess</h1>
			<div className={styles.buttons}>
				<button className={styles.button} onClick={playWithBot}>
					Play with Bot
				</button>
				<button className={styles.button} onClick={playWithFriend}>
					Play with Friend
				</button>
			</div>
		</div>
	)
}
